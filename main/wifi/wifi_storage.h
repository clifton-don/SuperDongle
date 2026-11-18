/**
 * @file    wifi_storage.h
 * @brief   WiFi 配置持久化存储接口
 *
 * 将用户成功连接的 WiFi 列表保存在 NVS 中，重启后自动重连。
 * 列表按最近成功连接优先排序，最多保存 max_wifi_num 条。
 *
 * @date    2026-07-24
 * @author  Mr.Tang
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 类型与配置
 * ================================================================ */

/** @brief WiFi 存储配置 */
typedef struct {
    const char *nvs_namespace;  /**< NVS 命名空间，默认 "wifi_store" */
    uint8_t     max_wifi_num;   /**< 最多保存多少条 WiFi，默认 5 */
} wifi_storage_config_t;

/** @brief WiFi 存储默认配置 */
#define WIFI_STORAGE_DEFAULT_CONFIG()                                          \
    (wifi_storage_config_t){                                                   \
        .nvs_namespace = "wifi_store",                                         \
        .max_wifi_num  = 5,                                                    \
    }

/* ================================================================
 * 公共 API
 * ================================================================ */

/**
 * @brief 初始化 WiFi 存储模块
 *
 * 可重复调用，仅第一次生效。
 * config 为 NULL 时使用默认配置（命名空间 "wifi_store"，最多 5 条）。
 *
 * @param config 配置指针，NULL 为使用 WIFI_STORAGE_DEFAULT_CONFIG
 * @return
 *      - ESP_OK         初始化成功
 *      - 其他           NVS 初始化错误码
 */
esp_err_t wifi_storage_init(const wifi_storage_config_t *config);

/**
 * @brief 读取所有已保存的 WiFi 配置
 *
 * 按最近成功连接优先排序。如果没有配置，返回 ESP_OK 且 *count_out = 0。
 *
 * @param configs  输出缓冲区，大小至少为 config->max_wifi_num
 * @param count_out 输出实际读取到的条目数
 * @return
 *      - ESP_OK              读取成功（可能为 0 条）
 *      - ESP_ERR_INVALID_ARG  参数为 NULL
 *      - ESP_ERR_INVALID_STATE 未初始化
 *      - 其他                nvs 操作错误码
 */
esp_err_t wifi_storage_load_all(wifi_config_t *configs, uint8_t *count_out);

/**
 * @brief WiFi 成功连接后更新存储列表
 *
 * 策略：
 *   - 若该 SSID 已存在：移到首位（保持其他顺序不变）
 *   - 若不存在且列表未满：插入首位
 *   - 若不存在且列表已满：插入首位并丢弃最后一个
 *
 * @param config 已成功连接的 WiFi 配置
 * @return
 *      - ESP_OK              更新成功
 *      - ESP_ERR_INVALID_ARG  参数为 NULL
 *      - ESP_ERR_INVALID_STATE 未初始化
 *      - ESP_ERR_NO_MEM       内存不足
 *      - 其他                nvs 操作错误码
 */
esp_err_t wifi_storage_on_connected(const wifi_config_t *config);

/**
 * @brief 按 SSID 删除已保存的 WiFi 配置
 *
 * 精确匹配 SSID（区分大小写）。如果删除后列表为空，擦除 NVS key。
 *
 * @param ssid 要删除的 SSID
 * @return
 *      - ESP_OK              删除成功
 *      - ESP_ERR_INVALID_ARG  ssid 为 NULL 或空串
 *      - ESP_ERR_INVALID_STATE 未初始化
 *      - ESP_ERR_NO_MEM       内存不足
 *      - 其他                nvs 操作错误码
 */
esp_err_t wifi_storage_delete_by_ssid(const char *ssid);

#ifdef __cplusplus
}
#endif
