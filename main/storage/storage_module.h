/**
 * @file    storage_module.h
 * @brief   文件系统存储接口
 *
 * 两类存储功能：
 *   - SPIFFS（内部 Flash，A/B 双分区，存储网页文件）
 *   - SD 卡 FATFS（外部 SD 卡，网盘功能）
 *
 * WiFi 配置 NVS 持久化已拆分至 wifi_storage.h。
 *
 * @date    2026-05-18
 * @author  Mr.Tang
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "sdmmc_cmd.h"

/* ================================================================
 * 挂载路径
 * ================================================================ */

#define STORAGE_SPIFFS_MOUNT_POINT "/spiffs"
#define STORAGE_FATFS_MOUNT_POINT  "/disk"

/* ================================================================
 * FATFS + SD 卡
 * ================================================================ */

esp_err_t      storage_fatfs_sdcard_init(void);
sdmmc_card_t*  storage_fatfs_get_sdcard(void);

/* ================================================================
 * SPIFFS + A/B 分区管理
 *
 * SPIFFS A/B 双分区设计：
 *   设备上有两个 SPIFFS 分区：spiffs_0、spiffs_1。
 *   当前使用哪个分区由 NVS "spiffs_ota" 命名空间中的 "active" 值决定。
 *   OTA 更新时写入非活跃分区，成功后再切换活跃标志。
 *   这样即使更新失败，仍可从旧分区启动，保证设备不砖。
 * ================================================================ */

esp_err_t      storage_spiffs_init(void);
void           storage_module_spiffs_get_usage_statistics(size_t *total, size_t *used);
bool           storage_module_spiffs_file_exists(const char *path);

/** @brief SPIFFS A/B 活跃分区管理 */
const char*    storage_spiffs_get_active_label(void);   /* 返回 "spiffs_0" 或 "spiffs_1" */
int            storage_spiffs_get_active_idx(void);     /* 返回 0=spiffs_0, 1=spiffs_1 */
esp_err_t      storage_spiffs_set_active(int idx);      /* 设置活跃分区（0 或 1） */

