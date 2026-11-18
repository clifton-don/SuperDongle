/**
 * @file    usb_hid_receiver.h
 * @brief   USB HID UDP 接收器接口
 *
 * 在 UDP 8888 端口监听来自 PC 端（Python 脚本）的 HID 输入事件，
 * 将单键事件转译为 Boot Keyboard 全报告，驱动 usb_hid_bridge 发送。
 *
 * 协议（小端序）:
 *   - 广播发现:      [0x00]              → 应答 "HID_ACK"
 *   - 鼠标移动:      [0x01, dx, dy]      → (buttons unchanged, dx, dy, 0)
 *   - 鼠标按键:      [0x02, mask]        → (mask, 0, 0, 0)
 *   - 鼠标滚轮:      [0x03, delta]       → (buttons unchanged, 0, 0, delta)
 *   - 键盘按下/抬起: [0x04, 1|0, 键码]  → Boot Keyboard Report
 *
 * @date    2026-05-28
 * @author  Mr.Tang
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动 HID UDP 接收器
 *
 * 创建 socket 绑定 8888 端口并启动接收任务。
 *
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t usb_hid_receiver_start(void);

/**
 * @brief 停止 HID UDP 接收器
 */
void usb_hid_receiver_stop(void);

#ifdef __cplusplus
}
#endif
