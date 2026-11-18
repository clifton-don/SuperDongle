/**
 * @file    http_ota_manager.h
 * @brief   HTTP OTA 升级管理器接口
 *
 * 提供远程固件（esp_ota_* API）和 SPIFFS A/B 双分区更新的能力，
 * 包括版本检查、异步下载、状态查询等功能。
 *
 * @date    2026-05-19
 * @author  Mr.Tang
 */

#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================== 配置宏 ============================================= */

/** @brief 版本信息 URL 最大长度 */
#define OTA_VERSION_URL_MAX   512

/** @brief 更新描述信息最大长度 */
#define OTA_DESC_MAX          512

/** @brief 默认 OTA version.json 地址 */
#define HTTP_OTA_MANAGER_VERSION_URL "https://superdongle.clif.top/firmware/version.json"

/* ========================================== 类型定义 ============================================= */

/**
 * @brief OTA 管理器配置
 *
 * @note state_cb 回调在 HTTP 事件上下文中触发，不可做阻塞操作。
 *       回调 state 含义：0=无更新，1=有更新，2=下载中，3=完成，-1=错误。
 */
typedef struct {
    char version_url[OTA_VERSION_URL_MAX]; /**< version.json 完整 URL */
    void (*state_cb)(int state, const char *msg); /**< OTA 状态变化回调 */
} http_ota_manager_config_t;

/** @brief OTA 管理器默认配置宏 */
#define HTTP_OTA_MANAGER_DEFAULT_CONFIG() \
    (http_ota_manager_config_t) {          \
        .version_url = "",                 \
        .state_cb    = NULL,               \
    }

/* ========================================== 公共 API ============================================= */

/**
 * @brief 初始化 OTA 管理器
 *
 * 读取当前固件版本，准备检查更新。
 * 可多次调用以更新配置。
 *
 * @param cfg 配置指针，NULL 则使用默认配置（version_url 为空，需后续设置）
 * @return
 *      - ESP_OK    初始化成功
 *      - 其他       参考日志错误码
 */
esp_err_t http_ota_manager_init(const http_ota_manager_config_t *cfg);

/**
 * @brief 检查远程版本
 *
 * 请求 version.json，解析并对比版本号，结果通过 s_ota 全局状态保存。
 *
 * @return
 *      - ESP_OK              检查完成（无论是否有更新）
 *      - ESP_ERR_INVALID_STATE OTA 正在进行中
 *      - 其他                 网络或解析错误
 */
esp_err_t http_ota_manager_check(void);

/**
 * @brief 启动更新
 *
 * 在独立任务中异步执行固件和/或 SPIFFS 的下载与刷写。
 *
 * @return
 *      - ESP_OK              已启动更新任务
 *      - ESP_FAIL            无可用更新或更新任务已在运行
 */
esp_err_t http_ota_manager_start_update(void);

/** @brief 取消正在进行的更新 */
esp_err_t http_ota_manager_cancel(void);

/** @brief WiFi 连接成功后自动初始化并检查更新（在独立任务中执行） */
esp_err_t http_ota_manager_auto_init(void);

/**
 * @brief 从 NVS 读取自定义 version_url
 *
 * 优先读取 NVS 中存储的 URL，若不存在则返回编译期默认值。
 *
 * @param buf  输出缓冲区
 * @param size 缓冲区大小
 * @return ESP_OK 成功，其他为 NVS 读取错误
 */
esp_err_t http_ota_manager_get_version_url(char *buf, size_t size);

/**
 * @brief 将自定义 version_url 写入 NVS
 *
 * 写入后下次 OTA 自动检查将使用新 URL。写入空字符串可清除自定义设置。
 *
 * @param url 新的 version.json URL，空字符串表示恢复默认
 * @return ESP_OK 成功，其他为 NVS 写入错误
 */
esp_err_t http_ota_manager_set_version_url(const char *url);

/* ========================================== 状态查询 API ============================================= */

/**
 * @name 版本与更新信息查询
 * @{
 */
const char *http_ota_manager_get_current_version(void); /**< 当前固件版本 */
const char *http_ota_manager_get_new_version(void);     /**< 远程最新版本 */
const char *http_ota_manager_get_update_desc(void);     /**< 更新说明 */
const char *http_ota_manager_get_update_url(void);      /**< 固件下载地址 */
const char *http_ota_manager_get_spiffs_url(void);      /**< SPIFFS 下载地址 */
/** @} */

/**
 * @name 更新状态查询
 * @{
 */
bool        http_ota_manager_has_firmware(void);            /**< 本次更新是否包含固件 */
bool        http_ota_manager_has_spiffs(void);              /**< 本次更新是否包含 SPIFFS */
bool        http_ota_manager_is_update_available(void);     /**< 是否有可用更新 */
bool        http_ota_manager_is_busy(void);                 /**< 是否正在更新中 */
bool        http_ota_manager_has_error(void);               /**< 上次更新是否出错 */
int         http_ota_manager_get_progress(void);            /**< 下载进度 0~100 */
const char *http_ota_manager_get_status_msg(void);          /**< 当前状态描述文本 */
/** @} */

#ifdef __cplusplus
}
#endif