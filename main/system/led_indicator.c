/**
 * @file    led_indicator.c
 * @brief   WS2812 RGB LED 状态指示器实现
 *
 * 通过 led_strip 组件（RMT 后端）驱动单颗 WS2812，
 * 以颜色与闪烁模式反映设备运行状态。
 *
 * 依赖：components/led_strip（ESP 官方组件，RMT 方式）
 *
 * @date    2026-05-18
 * @author  Mr.Tang
 */

#include "led_indicator.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "event_bus.h"
#include "event_defs.h"

static const char *TAG = "[led_indicator]";

/* ================================================================
 * 配置宏
 * ================================================================ */

/**
 * @brief LED 全局亮度系数（0-255）
 *
 * 实现方式：set_color_locked() 中每个 RGB 通道乘以 BRIGHTNESS/255 缩放。
 * @note led_strip 驱动无原生亮度控制，由应用层自行缩放。
 */
#define LED_BRIGHTNESS   15

/**
 * @brief 互斥锁最大阻塞时间（毫秒）
 *
 * 策略：
 * - 任务上下文调用时为阻塞等待（最多 500ms）；
 * - 定时器回调中使用 try-lock（timeout=0），因 timer daemon task 不可长时间阻塞。
 */
#define LOCK_TASK_TIMEOUT_MS   500

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief 单个状态的颜色与闪烁模式
 *
 * period_ms = 0 → 常亮；period_ms > 0 → 按周期闪烁（亮灭各半周期）。
 */
typedef struct {
    uint8_t  r, g, b;
    uint32_t period_ms;
} led_pattern_t;

#define ERR_HOLD_TICKS 40  /**< 红灯保持计数（40×50ms=2000ms） */

/* ================================================================
 * 模块级静态变量
 * ================================================================ */

static led_strip_handle_t   s_strip   = NULL;  /**< led_strip 实例句柄 */
static TimerHandle_t        s_timer   = NULL;  /**< 闪烁定时器（基础周期 50ms） */
static SemaphoreHandle_t    s_mutex   = NULL;  /**< 保护 RMT 访问的互斥锁 */

static led_state_t          s_state     = LED_STATE_BOOTING;
static bool                 s_on        = false; /**< LED 当前物理亮灭 */
static bool                 s_enabled   = true;  /**< LED 总开关状态 */
static int                  s_file_ops  = 0;     /**< 并发文件传输计数 */
static bool                 s_file_err  = false; /**< 当前传输轮次是否存在错误 */
static int                  s_err_hold  = 0;     /**< 红灯保持剩余 tick 数 */
static bool                 s_ota_errored = false; /**< 当前 OTA 轮次是否已上报错误 */

/**
 * @brief 各状态对应的颜色与闪烁参数（下标对齐 led_state_t）
 *
 * LED_STATE_CONNECT_FAILED / ERROR 均通过 s_err_hold 统一管理红灯倒计时，
 * period_ms = 0 表示倒计时期间保持常亮不闪烁。
 */
static const led_pattern_t s_patterns[] = {
    [LED_STATE_BOOTING]         = { 0,   0,   0,   0    },
    [LED_STATE_AP_MODE]         = { 0,   0,   255, 1000 },
    [LED_STATE_CONNECTING]      = { 0,   255, 0,   600  },
    [LED_STATE_CONNECTED]       = { 0,   255, 0,   0    },
    [LED_STATE_CONNECT_FAILED]  = { 255, 0,   0,   0    },
    [LED_STATE_FILE_OPERATION]  = { 255, 255, 255, 200  },
    [LED_STATE_OTA_UPDATING]    = { 255, 0,   255, 300  },
    [LED_STATE_ERROR]           = { 255, 0,   0,   0    },
};

/* ================================================================
 * 内部函数 —— 调用者必须已持有 s_mutex
 * ================================================================ */

/**
 * @brief 设置 LED 颜色（调用者需持有 s_mutex）
 *
 * 若 LED 总开关关闭，则清除 LED 输出而非设置颜色。
 *
 * @param r 红色通道值（0-255，经 BRIGHTNESS 缩放）
 * @param g 绿色通道值（0-255）
 * @param b 蓝色通道值（0-255）
 */
static void set_color_locked(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_strip) return;

    if (!s_enabled) {
        led_strip_clear(s_strip);
        led_strip_refresh(s_strip);
        return;
    }

    uint8_t r2 = (uint16_t)r * LED_BRIGHTNESS / 255;
    uint8_t g2 = (uint16_t)g * LED_BRIGHTNESS / 255;
    uint8_t b2 = (uint16_t)b * LED_BRIGHTNESS / 255;

    led_strip_set_pixel(s_strip, 0, r2, g2, b2);
    led_strip_refresh(s_strip);
}

/**
 * @brief 切换运行状态（调用者需持有 s_mutex）
 *
 * 与 led_indicator_set_state() 逻辑相同，仅不处理锁操作。
 *
 * @param state 目标运行状态
 */
static void internal_set_state(led_state_t state)
{
    if (state >= sizeof(s_patterns) / sizeof(s_patterns[0])) return;
    s_state = state;
    s_on = false;

    const led_pattern_t *p = &s_patterns[state];
    if (p->period_ms == 0) {
        xTimerChangePeriod(s_timer, pdMS_TO_TICKS(50), 0);
        set_color_locked(p->r, p->g, p->b);
        s_on = true;
    } else {
        xTimerChangePeriod(s_timer, pdMS_TO_TICKS(p->period_ms / 2), 0);
        set_color_locked(p->r, p->g, p->b);
    }
}

/**
 * @brief 触发红灯 2 秒倒计时
 *
 * 设置 s_err_hold 为 ERR_HOLD_TICKS（40 tick × 50ms = 2000ms），
 * 到期后定时器回调自动恢复绿色常亮。
 */
static void set_red_hold(void)
{
    s_err_hold = ERR_HOLD_TICKS;
    led_indicator_set_state(LED_STATE_ERROR);
}

/* ================================================================
 * 定时器回调
 * ================================================================ */

/**
 * @brief 闪烁定时器回调（50ms 基础周期）
 *
 * 使用 try-lock（xSemaphoreTake timeout=0），锁不可用时跳过本次 tick，
 * 因定时器运行在 timer daemon task 中不可长时间阻塞。
 *
 * 两种运行模式：
 * - 正常模式：常亮状态保持，闪烁状态交替亮灭；
 * - 错误保持模式（s_err_hold > 0）：递减计数，归零后自动恢复绿色。
 *
 * @param xt 定时器句柄（未使用）
 */
static void timer_cb(TimerHandle_t xt)
{
    (void)xt;

    if (s_mutex && xSemaphoreTake(s_mutex, 0) != pdTRUE) {
        return;
    }

    if (s_err_hold > 0) {
        s_err_hold--;
        if (s_err_hold == 0) {
            internal_set_state(LED_STATE_CONNECTED);
        }
        if (s_mutex) xSemaphoreGive(s_mutex);
        return;
    }

    const led_pattern_t *p = &s_patterns[s_state];
    if (p->period_ms == 0) {
        if (!s_on) {
            set_color_locked(p->r, p->g, p->b);
            s_on = true;
        }
        if (s_mutex) xSemaphoreGive(s_mutex);
        return;
    }
    s_on = !s_on;
    if (s_on) {
        set_color_locked(p->r, p->g, p->b);
    } else {
        led_strip_clear(s_strip);
    }
    if (s_mutex) xSemaphoreGive(s_mutex);
}

/* ================================================================
 * 事件总线处理
 * ================================================================ */

/** @brief WiFi 事件分发 */
static void led_on_wifi_event(const void *payload, size_t len, void *ctx)
{
    (void)payload; (void)len;
    superdongle_event_id_t evt = (superdongle_event_id_t)(uintptr_t)ctx;
    switch (evt) {
        case EVENT_WIFI_CONNECTING:
            led_indicator_wifi_event(LED_WIFI_CONNECTING);
            break;
        case EVENT_WIFI_CONNECTED:
            led_indicator_wifi_event(LED_WIFI_CONNECTED);
            break;
        case EVENT_WIFI_DISCONNECTED:
            led_indicator_wifi_event(LED_WIFI_DISCONNECTED);
            break;
        case EVENT_WIFI_CONNECT_FAILED:
            led_indicator_wifi_event(LED_WIFI_CONNECT_FAILED);
            break;
        default:
            break;
    }
}

/** @brief 文件事件分发 */
static void led_on_file_event(const void *payload, size_t len, void *ctx)
{
    (void)payload; (void)len;
    superdongle_event_id_t evt = (superdongle_event_id_t)(uintptr_t)ctx;
    switch (evt) {
        case EVENT_FILE_UPLOAD_STARTED:
            led_indicator_file_event(LED_FILE_STARTED);
            break;
        case EVENT_FILE_UPLOAD_FINISHED:
            led_indicator_file_event(LED_FILE_FINISHED);
            break;
        case EVENT_FILE_UPLOAD_FAILED:
            led_indicator_file_event(LED_FILE_FAILED);
            break;
        case EVENT_FILE_DOWNLOAD_STARTED:
            led_indicator_file_event(LED_FILE_STARTED);
            break;
        case EVENT_FILE_DOWNLOAD_FINISHED:
            led_indicator_file_event(LED_FILE_FINISHED);
            break;
        case EVENT_FILE_DOWNLOAD_FAILED:
            led_indicator_file_event(LED_FILE_FAILED);
            break;
        default:
            break;
    }
}

/** @brief OTA 事件分发 */
static void led_on_ota_event(const void *payload, size_t len, void *ctx)
{
    (void)payload; (void)len;
    superdongle_event_id_t evt = (superdongle_event_id_t)(uintptr_t)ctx;
    switch (evt) {
        case EVENT_OTA_STARTED:
            led_indicator_ota_event(LED_OTA_STARTED);
            break;
        case EVENT_OTA_FINISHED:
            led_indicator_ota_event(LED_OTA_FINISHED);
            break;
        case EVENT_OTA_ERROR:
            led_indicator_ota_event(LED_OTA_ERROR);
            break;
        default:
            break;
    }
}

/** @brief LED 使能事件 */
static void led_on_enable_event(const void *payload, size_t len, void *ctx)
{
    (void)ctx;
    if (payload && len >= sizeof(bool)) {
        led_indicator_set_enabled(*(const bool *)payload);
    }
}

/* ================================================================
 * 公共 API
 * ================================================================ */

/**
 * @brief 参见 led_indicator.h
 */
esp_err_t led_indicator_init(int gpio_pin)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;

    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio_pin,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        }
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,
        .mem_block_symbols = 0,
        .flags = {
            .with_dma = false,
        }
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_timer = xTimerCreate("led_tmr", pdMS_TO_TICKS(50), pdTRUE, NULL, timer_cb);
    if (!s_timer) return ESP_ERR_NO_MEM;

    xTimerStart(s_timer, 0);

    led_strip_clear(s_strip);
    led_strip_refresh(s_strip);

    /* 注册事件总线处理器（user_ctx 传递 event_id 用于区分事件类型） */
    event_bus_register(EVENT_WIFI_CONNECTING,      led_on_wifi_event, (void*)(uintptr_t)EVENT_WIFI_CONNECTING);
    event_bus_register(EVENT_WIFI_CONNECTED,       led_on_wifi_event, (void*)(uintptr_t)EVENT_WIFI_CONNECTED);
    event_bus_register(EVENT_WIFI_DISCONNECTED,    led_on_wifi_event, (void*)(uintptr_t)EVENT_WIFI_DISCONNECTED);
    event_bus_register(EVENT_WIFI_CONNECT_FAILED,  led_on_wifi_event, (void*)(uintptr_t)EVENT_WIFI_CONNECT_FAILED);
    event_bus_register(EVENT_FILE_UPLOAD_STARTED,   led_on_file_event, (void*)(uintptr_t)EVENT_FILE_UPLOAD_STARTED);
    event_bus_register(EVENT_FILE_UPLOAD_FINISHED,  led_on_file_event, (void*)(uintptr_t)EVENT_FILE_UPLOAD_FINISHED);
    event_bus_register(EVENT_FILE_UPLOAD_FAILED,    led_on_file_event, (void*)(uintptr_t)EVENT_FILE_UPLOAD_FAILED);
    event_bus_register(EVENT_FILE_DOWNLOAD_STARTED, led_on_file_event, (void*)(uintptr_t)EVENT_FILE_DOWNLOAD_STARTED);
    event_bus_register(EVENT_FILE_DOWNLOAD_FINISHED, led_on_file_event, (void*)(uintptr_t)EVENT_FILE_DOWNLOAD_FINISHED);
    event_bus_register(EVENT_FILE_DOWNLOAD_FAILED,  led_on_file_event, (void*)(uintptr_t)EVENT_FILE_DOWNLOAD_FAILED);
    event_bus_register(EVENT_OTA_STARTED,  led_on_ota_event, (void*)(uintptr_t)EVENT_OTA_STARTED);
    event_bus_register(EVENT_OTA_FINISHED, led_on_ota_event, (void*)(uintptr_t)EVENT_OTA_FINISHED);
    event_bus_register(EVENT_OTA_ERROR,    led_on_ota_event, (void*)(uintptr_t)EVENT_OTA_ERROR);
    event_bus_register(EVENT_LED_SET_ENABLED, led_on_enable_event, NULL);

    /* ---- 加载 NVS 配置（led_enabled），同时确保 NVS 已初始化 ---- */
    {
        esp_err_t nvs_ret = nvs_flash_init();
        if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            nvs_ret = nvs_flash_init();
        }
        /* ESP_ERR_NVS_INVALID_STATE 表示已初始化，忽略 */
        if (nvs_ret != ESP_OK && nvs_ret != ESP_ERR_NVS_INVALID_STATE) {
            ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(nvs_ret));
        }
    }
    led_indicator_load_nvs_config();

    ESP_LOGI(TAG, "LED ready on GPIO %d", gpio_pin);
    return ESP_OK;
}

/**
 * @brief 参见 led_indicator.h
 */
void led_indicator_load_nvs_config(void)
{
    nvs_handle_t nvs_h;
    esp_err_t nvs_ret = nvs_open("superdongle", NVS_READONLY, &nvs_h);
    if (nvs_ret == ESP_OK) {
        uint8_t val = 1;
        nvs_ret = nvs_get_u8(nvs_h, "led_enabled", &val);
        nvs_close(nvs_h);
        if (nvs_ret == ESP_OK || nvs_ret == ESP_ERR_NVS_NOT_FOUND) {
            s_enabled = (val != 0);
            ESP_LOGI(TAG, "LED enabled from NVS: %s (%s)",
                     s_enabled ? "true" : "false",
                     nvs_ret == ESP_ERR_NVS_NOT_FOUND ? "default" : "saved");
        } else {
            ESP_LOGE(TAG, "Failed to read LED state from NVS: %s", esp_err_to_name(nvs_ret));
        }
    } else {
        ESP_LOGW(TAG, "NVS namespace not found, using default LED settings: enabled");
    }
}

/**
 * @brief 参见 led_indicator.h
 */
void led_indicator_set_state(led_state_t state)
{
    if (!s_mutex) return;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(LOCK_TASK_TIMEOUT_MS)) != pdTRUE) {
        return;
    }
    internal_set_state(state);
    xSemaphoreGive(s_mutex);
}

/**
 * @brief 参见 led_indicator.h
 */
void led_indicator_set_enabled(bool enabled)
{
    if (!s_mutex) {
        s_enabled = enabled;
        return;
    }
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(LOCK_TASK_TIMEOUT_MS)) != pdTRUE) {
        return;
    }
    s_enabled = enabled;
    if (enabled) {
        internal_set_state(s_state);
    } else {
        led_strip_clear(s_strip);
        led_strip_refresh(s_strip);
        s_on = false;
    }
    xSemaphoreGive(s_mutex);
}

/**
 * @brief 参见 led_indicator.h
 */
bool led_indicator_is_enabled(void)
{
    return s_enabled;
}

/**
 * @brief 参见 led_indicator.h
 */
void led_indicator_file_event(led_file_event_t event)
{
    switch (event) {
    case LED_FILE_STARTED:
        if (s_file_ops == 0) {
            s_file_err = false;
            s_err_hold = 0;
            led_indicator_set_state(LED_STATE_FILE_OPERATION);
        }
        s_file_ops++;
        break;

    case LED_FILE_FINISHED:
        if (s_file_ops > 0) {
            s_file_ops--;
            if (s_file_ops == 0) {
                if (s_file_err) {
                    set_red_hold();
                } else {
                    s_err_hold = 0;
                    led_indicator_set_state(LED_STATE_CONNECTED);
                }
            }
        }
        break;

    case LED_FILE_FAILED:
        if (s_file_ops > 0) {
            s_file_err = true;
            s_file_ops--;
            if (s_file_ops == 0) {
                set_red_hold();
            }
        }
        break;
    }
}

/**
 * @brief 参见 led_indicator.h
 */
void led_indicator_ota_event(led_ota_event_t event)
{
    switch (event) {
    case LED_OTA_STARTED:
        s_ota_errored = false;
        s_err_hold = 0;
        led_indicator_set_state(LED_STATE_OTA_UPDATING);
        break;

    case LED_OTA_ERROR:
        if (!s_ota_errored) {
            s_ota_errored = true;
            set_red_hold();
        }
        break;

    case LED_OTA_CANCELLED:
        s_ota_errored = true;
        set_red_hold();
        break;

    case LED_OTA_FINISHED:
        s_err_hold = 0;
        led_indicator_set_state(LED_STATE_CONNECTED);
        break;
    }
}

/**
 * @brief 参见 led_indicator.h
 */
void led_indicator_wifi_event(led_wifi_event_t event)
{
    switch (event) {
    case LED_WIFI_CONNECTING:
        s_err_hold = 0;
        led_indicator_set_state(LED_STATE_CONNECTING);
        break;

    case LED_WIFI_CONNECTED:
        s_err_hold = 0;
        led_indicator_set_state(LED_STATE_CONNECTED);
        break;

    case LED_WIFI_CONNECT_FAILED:
        s_err_hold = 0;
        led_indicator_set_state(LED_STATE_CONNECT_FAILED);
        break;

    case LED_WIFI_DISCONNECTED:
        s_err_hold = 0;
        led_indicator_set_state(LED_STATE_AP_MODE);
        break;
    }
}
