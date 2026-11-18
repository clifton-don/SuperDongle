/**
 * @file    usb_hid_bridge.c
 * @brief   USB HID 键鼠桥接模块实现
 *
 * 通过 FreeRTOS 队列异步接收 HID 报告请求，在独立任务中
 * 调用 TinyUSB tud_hid_* API 发送到 USB 主机。
 * 报告描述符不使用 Report ID，每个 HID 实例对应一种设备类型。
 *
 * @date    2026-05-28
 * @author  Mr.Tang
 */

#include "usb_hid_bridge.h"

#include <string.h>

#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "[usb_hid]";

#if (CONFIG_TINYUSB_HID_COUNT > 0)

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "tusb.h"

#include "usb_descriptors.h"

/* ========================================== 类型定义 ============================================= */

/** @brief 桥接内部报告类型枚举 */
typedef enum {
    USB_HID_REPORT_KEYBOARD = 0,
    USB_HID_REPORT_MOUSE    = 1,
} usb_hid_report_type_t;

/** @brief HID 报告结构体（键盘/鼠标联合体） */
typedef struct {
    usb_hid_report_type_t type;
    union {
        struct {
            uint8_t modifier;       /**< 修饰键位掩码 */
            uint8_t keycode[6];     /**< 6 字节按键码 */
        } keyboard;
        struct {
            uint8_t buttons;        /**< 按键位掩码 */
            int8_t  x;              /**< X 轴位移 */
            int8_t  y;              /**< Y 轴位移 */
            int8_t  wheel;          /**< 滚轮位移 */
        } mouse;
    };
} hid_report_t;

/* ========================================== 模块级静态变量 ============================================= */

static QueueHandle_t s_hid_queue  = NULL;   /**< HID 报告发送队列 */
static TaskHandle_t  s_hid_task   = NULL;   /**< HID 发送任务句柄 */
static bool          s_hid_active = false;  /**< HID 桥接激活标志 */

/* ========================================== HID 发送任务 ============================================= */

/**
 * @brief HID 报告发送任务
 *
 * 从队列中取出报告，等待 TinyUSB HID 端点就绪后发送。
 *
 * @param arg 未使用
 */
static void hid_send_task(void *arg)
{
    (void)arg;
    hid_report_t report;

    while (1) {
        if (xQueueReceive(s_hid_queue, &report, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (report.type) {
            case USB_HID_REPORT_KEYBOARD: {
                TickType_t start = xTaskGetTickCount();
                while (!tud_hid_n_ready(ITF_NUM_HID_KB)) {
                    if (xTaskGetTickCount() - start > pdMS_TO_TICKS(100)) {
                        ESP_LOGW(TAG, "KB report timeout");
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(1));
                }
                if (tud_hid_n_ready(ITF_NUM_HID_KB)) {
                    tud_hid_n_keyboard_report(ITF_NUM_HID_KB, 0,
                                              report.keyboard.modifier,
                                              report.keyboard.keycode);
                }
                break;
            }
            case USB_HID_REPORT_MOUSE: {
                TickType_t start = xTaskGetTickCount();
                while (!tud_hid_n_ready(ITF_NUM_HID_MOUSE)) {
                    if (xTaskGetTickCount() - start > pdMS_TO_TICKS(100)) {
                        ESP_LOGW(TAG, "Mouse report timeout");
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(1));
                }
                if (tud_hid_n_ready(ITF_NUM_HID_MOUSE)) {
                    tud_hid_n_mouse_report(ITF_NUM_HID_MOUSE, 0,
                                           report.mouse.buttons,
                                           report.mouse.x,
                                           report.mouse.y,
                                           report.mouse.wheel,
                                           0);
                }
                break;
            }
        }
    }
}

/* ========================================== 公共 API ============================================= */

/**
 * @brief 参见 usb_hid_bridge.h
 */
esp_err_t usb_hid_bridge_init(void)
{
    if (s_hid_active) {
        return ESP_OK;
    }

    s_hid_queue = xQueueCreate(16, sizeof(hid_report_t));
    if (!s_hid_queue) {
        ESP_LOGE(TAG, "Failed to create HID queue");
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(hid_send_task, "hid_send", 3072, NULL,
                    tskIDLE_PRIORITY + 3, &s_hid_task) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create HID task");
        vQueueDelete(s_hid_queue);
        s_hid_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_hid_active = true;
    ESP_LOGI(TAG, "HID bridge started");
    return ESP_OK;
}

/**
 * @brief 参见 usb_hid_bridge.h
 */
esp_err_t usb_hid_bridge_deinit(void)
{
    if (!s_hid_active) {
        return ESP_OK;
    }

    s_hid_active = false;

    if (s_hid_task) {
        vTaskDelete(s_hid_task);
        s_hid_task = NULL;
    }

    if (s_hid_queue) {
        vQueueDelete(s_hid_queue);
        s_hid_queue = NULL;
    }

    ESP_LOGI(TAG, "HID bridge stopped");
    return ESP_OK;
}

/**
 * @brief 参见 usb_hid_bridge.h
 */
bool usb_hid_bridge_is_active(void)
{
    return s_hid_active;
}

/**
 * @brief 参见 usb_hid_bridge.h
 */
esp_err_t usb_hid_bridge_send_keyboard(uint8_t modifier, const uint8_t keycode[6])
{
    if (!s_hid_active || !s_hid_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    hid_report_t report = {
        .type = USB_HID_REPORT_KEYBOARD,
    };
    report.keyboard.modifier = modifier;
    memcpy(report.keyboard.keycode, keycode, 6);

    if (xQueueSend(s_hid_queue, &report, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

/**
 * @brief 参见 usb_hid_bridge.h
 */
esp_err_t usb_hid_bridge_send_mouse(uint8_t buttons, int8_t x, int8_t y, int8_t wheel)
{
    if (!s_hid_active || !s_hid_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    hid_report_t report = {
        .type = USB_HID_REPORT_MOUSE,
    };
    report.mouse.buttons = buttons;
    report.mouse.x       = x;
    report.mouse.y       = y;
    report.mouse.wheel   = wheel;

    if (xQueueSend(s_hid_queue, &report, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

#else  /* CONFIG_TINYUSB_HID_COUNT == 0 — HID 禁用，stub 实现 */

esp_err_t usb_hid_bridge_init(void)
{
    ESP_LOGW(TAG, "TinyUSB HID not enabled, bridge unavailable");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t usb_hid_bridge_deinit(void) { return ESP_OK; }

bool usb_hid_bridge_is_active(void) { return false; }

esp_err_t usb_hid_bridge_send_keyboard(uint8_t modifier, const uint8_t keycode[6])
{
    (void)modifier;
    (void)keycode;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t usb_hid_bridge_send_mouse(uint8_t buttons, int8_t x, int8_t y, int8_t wheel)
{
    (void)buttons;
    (void)x;
    (void)y;
    (void)wheel;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif /* CONFIG_TINYUSB_HID_COUNT > 0 */
