/**
 * @file    wifi_web_handler.h
 * @brief   WiFi Web handler — 配网与 WiFi API 路由注册
 *
 * 提供 WiFi 状态查询、扫描、连接、已保存列表管理等的 HTTP handler，
 * 通过 wifi_web_handler_init() 将路由注册到 Web 服务。
 *
 * @date    2026-07-26
 * @author  Mr.Tang
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册所有 WiFi 相关 HTTP 路由
 *
 * 注册以下路由到 Web 服务：
 *   GET  /api/wifi/status       — WiFi 状态
 *   GET  /api/wifi/saved        — 已保存 WiFi 列表
 *   GET  /api/wifi/scan         — 扫描附近 AP
 *   POST /api/wifi/connect      — 连接 WiFi
 *   POST /api/wifi/saved/delete — 删除已保存 WiFi
 *   POST /api/wifi/saved/connect— 连接已保存 WiFi
 *
 * @return
 *      - ESP_OK          注册成功
 *      - ESP_ERR_INVALID_ARG  参数无效
 */
void wifi_web_handler_init(void);

#ifdef __cplusplus
}
#endif
