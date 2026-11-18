/**
 * @file    system_web_handler.h
 * @brief   系统设置 Web handler（LED、mDNS、factory reset、reboot）
 *
 * @date    2026-07-24
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册系统设置相关的 HTTP 路由到 Web 服务
 *
 * 注册以下路由：
 *   - GET  /api/settings/led/state   → 查询 LED 开关状态
 *   - POST /api/settings/led/save    → 保存 LED 开关状态
 *   - POST /api/settings/mdns/save   → 保存 mDNS 主机名并重启
 *   - POST /api/system/factory_reset → 恢复出厂设置并重启
 *   - POST /api/system/reboot        → 重启设备
 */
void system_web_handler_init(void);

#ifdef __cplusplus
}
#endif
