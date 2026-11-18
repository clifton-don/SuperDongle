/**
 * @file    usb_hid_bridge.h
 * @brief   USB HID 键鼠桥接模块接口
 *
 * 提供键盘和鼠标 HID 报告的异步发送接口。
 * 内部通过 FreeRTOS 队列将报告传递给独立发送任务，
 * 任务中调用 TinyUSB tud_hid_* API 完成 USB 传输。
 *
 * @date    2026-05-28
 * @author  Mr.Tang
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================== 公共 API ============================================= */

/**
 * @brief 初始化 HID 桥接
 *
 * 创建发送队列和后台任务。
 *
 * @return
 *      - ESP_OK          初始化成功或已初始化
 *      - ESP_ERR_NO_MEM  队列或任务创建失败
 */
esp_err_t usb_hid_bridge_init(void);

/**
 * @brief 反初始化 HID 桥接
 *
 * 删除后台任务和队列。
 *
 * @return ESP_OK
 */
esp_err_t usb_hid_bridge_deinit(void);

/**
 * @brief 查询 HID 桥接运行状态
 *
 * @return true=运行中，false=未运行
 */
bool usb_hid_bridge_is_active(void);

/**
 * @brief 发送键盘 HID 报告（异步入队）
 *
 * 复制 keycode 到内部缓冲区后入队，调用者无需保持 keycode 有效。
 *
 * @param modifier 修饰键位掩码（bit0=CTRL, bit1=SHIFT, bit2=ALT, bit3=GUI）
 * @param keycode  6 字节按键码数组，未使用位置填 0
 * @return
 *      - ESP_OK              入队成功
 *      - ESP_ERR_INVALID_STATE 桥接未运行
 *      - ESP_ERR_TIMEOUT     队列满，丢弃本次报告
 */
esp_err_t usb_hid_bridge_send_keyboard(uint8_t modifier, const uint8_t keycode[6]);

/**
 * @brief 发送鼠标 HID 报告（异步入队）
 *
 * @param buttons 按键状态位掩码（bit0=左键, bit1=右键, bit2=中键）
 * @param x       X 轴相对位移（-127 ~ 127）
 * @param y       Y 轴相对位移（-127 ~ 127）
 * @param wheel   滚轮位移（-127 ~ 127）
 * @return
 *      - ESP_OK              入队成功
 *      - ESP_ERR_INVALID_STATE 桥接未运行
 *      - ESP_ERR_TIMEOUT     队列满，丢弃本次报告
 */
esp_err_t usb_hid_bridge_send_mouse(uint8_t buttons, int8_t x, int8_t y, int8_t wheel);

#ifdef __cplusplus
}
#endif
