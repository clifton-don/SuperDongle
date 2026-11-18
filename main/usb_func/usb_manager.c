/**
 * @file    usb_manager.c
 * @brief   USB 单功能管理模块实现
 *
 * 统一管理 USB 模式切换（MSC ↔ HID），每次仅运行一种功能。
 * 模式通过 NVS 持久化到 "superdongle" 命名空间，键名 "usb_mode"。
 *
 * 切换流程：
 *   1. 停止当前桥接模块
 *   2. tinyusb_driver_uninstall()
 *   3. usb_descriptors_set_for_mode() 更新描述符
 *   4. tinyusb_driver_install() 安装 USB 驱动
 *   5. 启动目标模式的桥接模块
 *   6. usb_manager_nvs_save_mode() 写入 NVS
 *
 * @date    2026-05-28
 * @author  Mr.Tang
 */

#include "usb_manager.h"

#include "esp_log.h"
#include "nvs.h"
#include "sdkconfig.h"

#include "event_bus.h"
#include "event_defs.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"

#include "storage_module.h"
#include "usb_descriptors.h"
#include "usb_hid_bridge.h"
#include "usb_hid_receiver.h"
#include "usb_ncm.h"

static const char *TAG = "[usb_mgr]";

/* ========================================== 配置宏 ============================================= */

#define USB_MODE_NVS_NAMESPACE  "superdongle"  /**< NVS 命名空间 */
#define USB_MODE_NVS_KEY        "usb_mode"     /**< NVS 键名 */
#define USB_UNINSTALL_DELAY_MS  200            /**< 卸载后稳定延时（毫秒） */

/* ========================================== 模块级静态变量 ============================================= */

static usb_manager_mode_t       s_current_mode    = USB_MODE_MSC_ONLY;  /**< 当前运行模式 */
static bool                     s_initialized      = false;             /**< 管理器初始化标志 */
static tinyusb_msc_storage_handle_t s_msc_handle  = NULL;              /**< MSC 存储句柄 */
static bool                     s_msc_storage_ok   = false;             /**< MSC 存储就绪标志 */
static bool                     s_msc_driver_ok    = false;             /**< MSC driver 首次安装后复用，避免重复安装返回 ESP_ERR_INVALID_STATE */
static usb_manager_bridge_stop_cb_t s_bridge_stop_cb = NULL;           /**< 桥接停止回调 */
static int                      s_pending_mode    = -1;                 /**< 预切换模式，-1=无，供网页提前获知 */
static bool                     s_switching      = false;              /**< 切换互斥锁，防止并发调用 set_mode */

/* ========================================== NVS 持久化 ============================================= */

/**
 * @brief 将当前模式写入 NVS
 *
 * @param mode 待保存的模式
 * @return ESP_OK 成功，或其他错误码
 */
static esp_err_t usb_manager_nvs_save_mode(usb_manager_mode_t mode)
{
    nvs_handle_t nvs_h;
    esp_err_t ret = nvs_open(USB_MODE_NVS_NAMESPACE, NVS_READWRITE, &nvs_h);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_u8(nvs_h, USB_MODE_NVS_KEY, (uint8_t)mode);
    if (ret == ESP_OK) {
        nvs_commit(nvs_h);
    }
    nvs_close(nvs_h);
    return ret;
}

/**
 * @brief 从 NVS 读取已保存的模式
 *
 * @param default_mode NVS 无记录时的回退值
 * @return 已保存的模式或 default_mode
 */
static usb_manager_mode_t usb_manager_nvs_load_mode(usb_manager_mode_t default_mode)
{
    nvs_handle_t nvs_h;
    esp_err_t ret = nvs_open(USB_MODE_NVS_NAMESPACE, NVS_READONLY, &nvs_h);
    if (ret != ESP_OK) {
        return default_mode;
    }

    uint8_t val = (uint8_t)default_mode;
    ret = nvs_get_u8(nvs_h, USB_MODE_NVS_KEY, &val);
    nvs_close(nvs_h);

    if (ret != ESP_OK || val >= USB_MODE_COUNT) {
        return default_mode;
    }

    return (usb_manager_mode_t)val;
}

/* ========================================== MSC 存储初始化 ============================================= */

/**
 * @brief 初始化 MSC 存储子系统
 *
 * 安装 TinyUSB MSC 驱动并挂载 SD 卡为 USB 存储。
 * 仅需调用一次，重复调用幂等返回。
 *
 * @return ESP_OK 成功，或其他错误码
 */
static esp_err_t usb_manager_msc_storage_init(void)
{
    if (s_msc_storage_ok) {
        return ESP_OK;
    }

    // MSC driver only installed once, never uninstalled
    if (!s_msc_driver_ok) {
        tinyusb_msc_driver_config_t driver_config = {
            .user_flags.val  = 0,
            .callback        = NULL,
            .callback_arg    = NULL,
        };

        esp_err_t ret = tinyusb_msc_install_driver(&driver_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "tinyusb_msc_install_driver failed: %s", esp_err_to_name(ret));
            return ret;
        }
        s_msc_driver_ok = true;
    }

    sdmmc_card_t *card = storage_fatfs_get_sdcard();
    if (card == NULL) {
        if (storage_fatfs_sdcard_init() != ESP_OK) {
            ESP_LOGE(TAG, "SD card init failed");
            return ESP_FAIL;
        }
        card = storage_fatfs_get_sdcard();
        if (card == NULL) {
            ESP_LOGE(TAG, "SD card handle is NULL");
            return ESP_FAIL;
        }
    }

    tinyusb_msc_storage_config_t storage_config = {
        .medium.card = card,
        .fat_fs = {
            .base_path = STORAGE_FATFS_MOUNT_POINT,
            .config = {
                .format_if_mount_failed = true,
                .max_files               = 3,
                .allocation_unit_size     = 16 * 1024,
            },
            .do_not_format = false,
            .format_flags  = 0,
        },
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB,
    };

    esp_err_t ret = tinyusb_msc_new_storage_sdmmc(&storage_config, &s_msc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_msc_new_storage_sdmmc failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_msc_storage_ok = true;
    ESP_LOGI(TAG, "MSC storage initialized");
    return ESP_OK;
}

/* ========================================== USB 驱动安装/卸载 ============================================= */

/**
 * @brief 按指定模式安装 TinyUSB 驱动
 *
 * @param mode 目标模式
 * @return ESP_OK 成功，或其他错误码
 */
static esp_err_t usb_manager_driver_install_for_mode(usb_manager_mode_t mode)
{
    usb_descriptors_set_for_mode(mode);

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG(NULL);
    tusb_cfg.descriptor = *usb_descriptors_get_config();

    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_driver_install failed for mode %d: %s",
                 (int)mode, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "USB driver installed for mode %d", (int)mode);
    return ESP_OK;
}

/**
 * @brief 拆除 TinyUSB 驱动
 */
static void usb_manager_driver_uninstall(void)
{
    // Delete MSC storage (keep driver intact to avoid crash from pending USB events)
    if (s_msc_handle != NULL) {
        tinyusb_msc_delete_storage(s_msc_handle);
        s_msc_handle = NULL;
        s_msc_storage_ok = false;
    }
    tinyusb_driver_uninstall();
    vTaskDelay(pdMS_TO_TICKS(USB_UNINSTALL_DELAY_MS));
    ESP_LOGI(TAG, "USB driver uninstalled");
}

/* ========================================== 桥接模块管理 ============================================= */

/**
 * @brief 根据模式启动对应的桥接功能
 *
 * @param mode 目标模式
 * @return ESP_OK 成功，启动失败仅打日志不阻断流程
 */
static esp_err_t usb_manager_bridge_start_for_mode(usb_manager_mode_t mode)
{
    if (mode == USB_MODE_HID_ONLY) {
#if (CONFIG_TINYUSB_HID_COUNT > 0)
        if (!usb_hid_bridge_is_active()) {
            esp_err_t ret = usb_hid_bridge_init();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "HID bridge start failed: %s", esp_err_to_name(ret));
                return ret;
            }
            usb_hid_receiver_start();
        }
#else
        ESP_LOGW(TAG, "TinyUSB HID not configured, cannot start HID bridge");
        return ESP_ERR_NOT_SUPPORTED;
#endif
    } else if (mode == USB_MODE_NCM_ONLY) {
#if CONFIG_TINYUSB_NET_MODE_NCM
        /* 延迟 500ms 启动 NCM 桥接，确保 HTTP 响应先发送（切换后网页将不可用） */
        esp_err_t ret = usb_ncm_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "NCM bridge start failed: %s", esp_err_to_name(ret));
            return ret;
        }
#else
        ESP_LOGW(TAG, "TinyUSB NCM not configured, cannot start NCM bridge");
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }
    return ESP_OK;
}

/**
 * @brief 停止所有正在运行的桥接功能
 */
static void usb_manager_bridge_stop_all(void)
{
    // 通知外部模块关闭 HID 相关资源（如 WebSocket）
    if (s_bridge_stop_cb) {
        s_bridge_stop_cb();
    }

#if (CONFIG_TINYUSB_HID_COUNT > 0)
    if (usb_hid_bridge_is_active()) {
        ESP_LOGI(TAG, "Stopping HID receiver...");
        usb_hid_receiver_stop();
        ESP_LOGI(TAG, "Deiniting HID bridge...");
        usb_hid_bridge_deinit();
        ESP_LOGI(TAG, "HID bridge stopped");
    }
#endif

#if CONFIG_TINYUSB_NET_MODE_NCM
    ESP_LOGI(TAG, "Deiniting NCM bridge...");
    usb_ncm_deinit();
    ESP_LOGI(TAG, "NCM bridge stopped");

    /* 离开 NCM 模式时断开 WiFi，触发自动重连以恢复 IP 栈 */
    if (s_current_mode == USB_MODE_NCM_ONLY) {
        event_bus_post(EVENT_NCM_EXIT, NULL, 0);
    }
#endif
}

/* ========================================== 事件总线处理 ============================================= */

/** @brief 处理 USB 模式切换请求（来自按键模块） */
static void usb_on_switch_request(const void *payload, size_t len, void *ctx)
{
    (void)ctx;
    if (payload && len >= sizeof(int)) {
        int target_mode = *(const int *)payload;
        if (target_mode >= 0 && target_mode < USB_MODE_COUNT) {
            usb_manager_set_mode((usb_manager_mode_t)target_mode);
        }
    }
}

/* ========================================== 公共 API ============================================= */

/**
 * @brief 参见 usb_manager.h
 */
esp_err_t usb_manager_init(const usb_manager_config_t *config)
{
    if (s_initialized) {
        return ESP_OK;
    }

    usb_manager_config_t cfg;
    if (config == NULL) {
        cfg = USB_MANAGER_DEFAULT_CONFIG();
    } else {
        cfg = *config;
    }

    s_current_mode = usb_manager_nvs_load_mode(cfg.default_mode);

    usb_descriptors_init();

    if (s_current_mode == USB_MODE_MSC_ONLY) {
        esp_err_t ret = usb_manager_msc_storage_init();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "MSC storage init failed: %s, MSC will be unavailable", esp_err_to_name(ret));
            s_msc_storage_ok = false;
        }
    }

    esp_err_t ret = usb_manager_driver_install_for_mode(s_current_mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = usb_manager_bridge_start_for_mode(s_current_mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bridge start failed during init: %s", esp_err_to_name(ret));
        tinyusb_driver_uninstall();
        return ret;
    }

    s_initialized = true;

    /* 注册事件总线处理器 */
    event_bus_register(EVENT_USB_SWITCH_REQUEST, usb_on_switch_request, NULL);

    ESP_LOGI(TAG, "USB manager initialized, mode=%d (%s)",
             (int)s_current_mode, usb_manager_mode_to_string(s_current_mode));
    return ESP_OK;
}

/**
 * @brief 参见 usb_manager.h
 */
esp_err_t usb_manager_set_mode(usb_manager_mode_t mode)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (mode >= USB_MODE_COUNT) {
        ESP_LOGE(TAG, "Invalid mode: %d", (int)mode);
        return ESP_ERR_INVALID_ARG;
    }

    if (mode == s_current_mode) {
        ESP_LOGI(TAG, "Already in mode %d (%s), no change",
                 (int)mode, usb_manager_mode_to_string(mode));
        return ESP_OK;
    }

    if (s_switching) {
        ESP_LOGW(TAG, "Switch already in progress, rejecting request");
        return ESP_ERR_INVALID_STATE;
    }
    s_switching = true;

    ESP_LOGI(TAG, "Switching mode: %s → %s",
             usb_manager_mode_to_string(s_current_mode),
             usb_manager_mode_to_string(mode));

    usb_manager_mode_t old_mode = s_current_mode;

    ESP_LOGI(TAG, "Stopping bridge...");
    usb_manager_bridge_stop_all();
    ESP_LOGI(TAG, "Bridge stopped, uninstalling driver...");
    usb_manager_driver_uninstall();
    ESP_LOGI(TAG, "Driver uninstalled");

    if (mode == USB_MODE_MSC_ONLY) {
        esp_err_t ret = usb_manager_msc_storage_init();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "MSC storage init failed on switch: %s, continuing without storage", esp_err_to_name(ret));
            s_msc_storage_ok = false;
            // 不回退旧模式，降级启动 MSC 模式，前端显示报错提示
        }
    }

    esp_err_t ret = usb_manager_driver_install_for_mode(mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Driver reinstall failed: %s", esp_err_to_name(ret));
        // 回退旧模式驱动
        usb_manager_driver_install_for_mode(old_mode);
        usb_manager_bridge_start_for_mode(old_mode);
        s_switching = false;
        return ret;
    }

    s_current_mode = mode;
    s_pending_mode = -1;  // 切换完成，清除预通知

    ret = usb_manager_bridge_start_for_mode(s_current_mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bridge start failed: %s", esp_err_to_name(ret));
        // 回退旧模式
        usb_manager_driver_uninstall();
        usb_manager_driver_install_for_mode(old_mode);
        s_current_mode = old_mode;
        usb_manager_bridge_start_for_mode(old_mode);
        s_switching = false;
        return ret;
    }

    usb_manager_nvs_save_mode(mode);

    ESP_LOGI(TAG, "Mode switched to %d (%s)", (int)mode, usb_manager_mode_to_string(mode));
    event_bus_post(EVENT_USB_MODE_CHANGED, &mode, sizeof(mode));
    s_switching = false;
    return ESP_OK;
}

/**
 * @brief 参见 usb_manager.h
 */
usb_manager_mode_t usb_manager_get_mode(void)
{
    return s_current_mode;
}

int usb_manager_get_pending_mode(void)
{
    return s_pending_mode;
}

void usb_manager_set_pending_mode(int mode)
{
    s_pending_mode = mode;
    if (mode >= 0) {
        event_bus_post(EVENT_USB_MODE_PENDING, &mode, sizeof(mode));
    }
}

/**
 * @brief 参见 usb_manager.h
 */
const char *usb_manager_mode_to_string(usb_manager_mode_t mode)
{
    switch (mode) {
        case USB_MODE_MSC_ONLY:  return "MSC";
        case USB_MODE_HID_ONLY:  return "HID";
        case USB_MODE_NCM_ONLY:  return "NCM";
        default:                 return "UNKNOWN";
    }
}

/**
 * @brief 参见 usb_manager.h
 */
void usb_manager_register_bridge_stop_callback(usb_manager_bridge_stop_cb_t cb)
{
    s_bridge_stop_cb = cb;
}

/**
 * @brief 参见 usb_manager.h
 */
esp_err_t usb_manager_restart(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_switching) {
        ESP_LOGW(TAG, "Switch already in progress, rejecting restart");
        return ESP_ERR_INVALID_STATE;
    }
    s_switching = true;

    usb_manager_bridge_stop_all();
    usb_manager_driver_uninstall();

    // MSC 模式下重建存储（usb_manager_driver_uninstall 会清除）
    if (s_current_mode == USB_MODE_MSC_ONLY) {
        esp_err_t ret = usb_manager_msc_storage_init();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "MSC storage re-init failed on restart: %s", esp_err_to_name(ret));
        }
    }

    esp_err_t ret = usb_manager_driver_install_for_mode(s_current_mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Restart driver install failed: %s", esp_err_to_name(ret));
        s_switching = false;
        return ret;
    }

    ret = usb_manager_bridge_start_for_mode(s_current_mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Restart bridge start failed: %s", esp_err_to_name(ret));
        s_switching = false;
        return ret;
    }
    ESP_LOGI(TAG, "USB restarted in mode %d (%s)",
             (int)s_current_mode, usb_manager_mode_to_string(s_current_mode));
    s_switching = false;
    return ESP_OK;
}

/* ========================================== HID API 代理 ============================================= */

/**
 * @brief 参见 usb_manager.h
 */
bool usb_hid_is_active(void)
{
    return usb_hid_bridge_is_active();
}

/**
 * @brief 参见 usb_manager.h
 */
esp_err_t usb_hid_keyboard_send(uint8_t modifier, const uint8_t keycode[6])
{
    return usb_hid_bridge_send_keyboard(modifier, keycode);
}

/**
 * @brief 参见 usb_manager.h
 */
esp_err_t usb_hid_mouse_send(uint8_t buttons, int8_t x, int8_t y, int8_t wheel)
{
    return usb_hid_bridge_send_mouse(buttons, x, y, wheel);
}
