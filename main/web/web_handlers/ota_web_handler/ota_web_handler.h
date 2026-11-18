/**
 * @file    ota_web_handler.h
 * @brief   OTA 升级 Web handler
 *
 * @date    2026-07-24
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册 OTA 相关的 HTTP 路由到 Web 服务
 *
 * 注册以下路由：
 *   - GET  /api/ota/status       → 查询 OTA 升级进度和状态
 *   - POST /api/ota/check        → 触发 OTA 更新检查
 *   - POST /api/ota/update       → 启动 OTA 更新流程
 *   - POST /api/ota/cancel       → 取消正在进行的 OTA 更新
 *   - GET  /api/ota/config       → 获取 OTA version_url 配置
 *   - POST /api/ota/config/set   → 设置 OTA version_url 配置
 */
void ota_web_handler_init(void);

#ifdef __cplusplus
}
#endif
