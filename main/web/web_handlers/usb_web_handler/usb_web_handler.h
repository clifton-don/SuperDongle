/**
 * @file    usb_web_handler.h
 * @brief   USB/HID Web handler
 *
 * @date    2026-07-24
 */

#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册 USB/HID 相关的 HTTP 路由到 Web 服务
 *
 * 注册以下路由：
 *   - GET  /api/usb/status  → 查询 USB 运行模式
 *   - POST /api/usb/mode    → 切换 USB 运行模式
 *   - GET  /api/hid/ws      → HID WebSocket 二进制帧端点 (is_websocket=true)
 */
void usb_web_handler_init(void);

/**
 * @brief 关闭当前 HID WebSocket 连接（模式切换/桥接停止时调用）
 */
void usb_web_close_hid_ws(void);

/**
 * @brief 初始化粘贴打字任务（创建队列和后台任务）
 *
 * @return ESP_OK 成功，ESP_ERR_NO_MEM 资源不足
 */
esp_err_t paste_task_init(void);

/**
 * @brief 获取 Web 服务器句柄
 *
 * @return httpd_handle_t HTTP 服务器句柄
 */
httpd_handle_t web_module_get_server_handle(void);

#ifdef __cplusplus
}
#endif
