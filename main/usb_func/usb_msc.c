/**
 * @file    usb_msc.c
 * @brief   USB 大容量存储设备实现
 *
 * 基于 TinyUSB MSC 驱动，将 storage_module 初始化的 SD 卡
 * 通过 USB 暴露为 U 盘设备，供计算机直接访问。
 *
 * @date    2026-05-19
 * @author  Mr.Tang
 */

#include "usb_msc.h"

#include <stdio.h>

#include "esp_log.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"

#include "storage_module.h"

static const char *TAG = "[usb_msc]";

static tinyusb_msc_storage_handle_t s_msc_handle = NULL; /**< MSC 存储句柄 */

/* ========================================== 公共 API ============================================= */

/**
 * @brief 参见 usb_msc.h
 */
esp_err_t usb_msc_init(void)
{
    ESP_LOGI(TAG, "Initializing USB MSC...");

    /* 1. 安装 TinyUSB MSC 驱动 */
    tinyusb_msc_driver_config_t driver_config = {
        .user_flags.val = 0,
        .callback = NULL,
        .callback_arg = NULL
    };

    esp_err_t ret = tinyusb_msc_install_driver(&driver_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TinyUSB MSC driver: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 2. 检查 SD 卡是否已初始化 */
    sdmmc_card_t *card = storage_fatfs_get_sdcard();
    if (card == NULL) {
        if (storage_fatfs_sdcard_init() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize SD card");
            return ESP_FAIL;
        }
        card = storage_fatfs_get_sdcard();
        if (card == NULL) {
            ESP_LOGE(TAG, "Failed to get SD card handle");
            return ESP_FAIL;
        }
    }

    /* 3. 创建 MSC 存储 */
    tinyusb_msc_storage_config_t storage_config = {
        .medium.card = card,
        .fat_fs = {
            .base_path = STORAGE_FATFS_MOUNT_POINT,
            .config = {
                .format_if_mount_failed = true,
                .max_files = 3,
                .allocation_unit_size = 16 * 1024
            },
            .do_not_format = false,
            .format_flags = 0
        },
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB
    };

    ret = tinyusb_msc_new_storage_sdmmc(&storage_config, &s_msc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create MSC storage: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 4. 安装 TinyUSB 底层驱动 */
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG(NULL);

    ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TinyUSB driver: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "USB MSC initialized successfully! Connect to computer via USB.");
    return ESP_OK;
}

/**
 * @brief 参见 usb_msc.h
 */
esp_err_t usb_msc_restart(void)
{
    ESP_LOGI(TAG, "Restarting USB MSC device...");

    tinyusb_driver_uninstall();
    vTaskDelay(pdMS_TO_TICKS(100));

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG(NULL);
    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "USB MSC device restarted successfully");
    } else {
        ESP_LOGE(TAG, "Failed to restart USB MSC device: %s", esp_err_to_name(ret));
    }

    return ret;
}