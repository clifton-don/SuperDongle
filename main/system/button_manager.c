/**
 * @file    button_manager.c
 * @brief   物理按键管理模块实现
 *
 * 独立 FreeRTOS 任务轮询 BOOT0 按键状态：
 *   按下不放 → 1 秒时发布 EVENT_USB_MODE_PENDING 预通知
 *              3 秒后自动循环切换 USB 模式（MSC → HID → NCM → MSC → …）
 *   提前松开 → 无动作
 *   必须完全松开才能进行下一次按下
 *
 * 使用多次采样去抖，防误触。
 *
 * @date    2026-06-08
 * @author  Mr.Tang
 */

#include "button_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include "event_bus.h"
#include "event_defs.h"

static const char *TAG = "[button]";

/** @brief 当前 USB 模式缓存（通过事件总线被动更新） */
static int s_cur_mode = 0;

/* ========================================== 按键轮询任务 ============================================= */

static void button_poll_task(void *pvParameters)
{
    (void)pvParameters;

    /* 配置 GPIO */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_BOOT_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = (BUTTON_ACTIVE_LEVEL == 0) ? GPIO_PULLUP_ENABLE : GPIO_PULLDOWN_ENABLE,
        .pull_down_en = (BUTTON_ACTIVE_LEVEL == 0) ? GPIO_PULLDOWN_DISABLE : GPIO_PULLUP_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    bool prev_stable   = false;        /* 上一次去抖后的稳定电平 */
    bool pressed       = false;        /* 是否处于已去抖的按下状态 */
    bool hold_handled  = false;        /* 本次按下是否已触发 3s 切换（防重复） */
    bool pending_notified = false;     /* 本次按下是否已预通知 pending 模式 */
    TickType_t press_tick = 0;         /* 按下时刻的 tick */

    ESP_LOGI(TAG, "Button poll task started, GPIO=%d", BUTTON_BOOT_GPIO);

    while (1) {
        /* 3 次采样去抖，至少 2 次一致才确认电平 */
        int cnt = 0;
        for (int i = 0; i < 3; i++) {
            if (gpio_get_level(BUTTON_BOOT_GPIO) == BUTTON_ACTIVE_LEVEL) {
                cnt++;
            }
            vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS / 3));
        }
        bool level = (cnt >= 2);

        /* --- 上升沿：释放（必须松开才能再次按下） --- */
        if (!level && prev_stable && pressed) {
            pressed      = false;
            hold_handled = false;
            pending_notified = false;
            ESP_LOGD(TAG, "Released");
        }
            /* --- 下降沿：按下，开始计时 --- */
        else if (level && !prev_stable && !pressed) {
            pressed       = true;
            press_tick    = xTaskGetTickCount();
            hold_handled  = false;
            pending_notified = false;
            ESP_LOGD(TAG, "Pressed, starting 3s timer");
        }
        /* --- 持续按住：1s 预通知 + 3s 切换 --- */
        else if (level && pressed) {
            uint32_t elapsed = (xTaskGetTickCount() - press_tick) * portTICK_PERIOD_MS;

            // 1s: 预通知 pending 模式（网页端通过 HTTP 轮询获取，不更新本地缓存）
            if (elapsed >= 1000 && !pending_notified) {
                pending_notified = true;
                int next;
                switch (s_cur_mode) {
                    case 0:  next = 1;  break;   // MSC → HID
                    case 1:  next = 2;  break;   // HID → NCM
                    case 2:
                    default: next = 0;  break;   // NCM → MSC
                }
                event_bus_post(EVENT_USB_MODE_PENDING, &next, sizeof(next));
                ESP_LOGI(TAG, "1s hold: pre-notify pending mode=%d", next);
            }

            // 3s: 真正切换
            if (elapsed >= BUTTON_LONG_PRESS_MS && !hold_handled) {
                hold_handled = true;

                int next;
                switch (s_cur_mode) {
                    case 0:  next = 1;  break;
                    case 1:  next = 2;  break;
                    case 2:
                    default: next = 0;  break;
                }

                ESP_LOGI(TAG, "3s hold: switching %d → %d", s_cur_mode, next);
                event_bus_post(EVENT_USB_SWITCH_REQUEST, &next, sizeof(next));
            }
        }

        prev_stable = level;
        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_INTERVAL_MS));
    }
}

/* ========================================== 事件处理 ============================================= */

/**
 * @brief USB 模式变更时更新模式缓存
 */
static void btn_on_usb_mode_changed(const void *payload, size_t len, void *ctx)
{
    (void)ctx;
    if (payload && len >= sizeof(int)) {
        s_cur_mode = *(const int *)payload;
        ESP_LOGD(TAG, "USB mode cache updated: %d", s_cur_mode);
    }
}

/* ========================================== 公共 API ============================================= */

esp_err_t button_manager_init(void)
{
    /* 注册事件总线监听：USB 模式变更时刷新本地缓存 */
    event_bus_register(EVENT_USB_MODE_CHANGED, btn_on_usb_mode_changed, NULL);

    BaseType_t ret = xTaskCreate(
        button_poll_task,
        "button_poll",
        3072,           /* 栈大小 */
        NULL,
        2,              /* 优先级（低于关键任务） */
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button poll task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Button manager initialized");
    return ESP_OK;
}
