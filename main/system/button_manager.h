/**
 * @file    button_manager.h
 * @brief   物理按键管理模块接口
 *
 * 管理 BOOT0 按键：
 *   按下不放 → 3 秒后自动循环切换 USB 模式（MSC → HID → NCM → MSC → …）
 *   提前松开 → 无动作
 *   必须完全松开才能进行下一次按下
 *
 * 独立任务运行，不受其他模块影响。
 *
 * @date    2026-06-08
 * @author  Mr.Tang
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================== 按键引脚配置 ============================================= */

/** @brief BOOT0 按键 GPIO 编号（ESP32-S3 默认 BOOT 按钮为 GPIO 0） */
#define BUTTON_BOOT_GPIO            0

/** @brief 按键按下电平（0 = 低电平有效，1 = 高电平有效） */
#define BUTTON_ACTIVE_LEVEL         0

/** @brief 长按判定时间（毫秒） */
#define BUTTON_LONG_PRESS_MS        3000

/** @brief 去抖采样窗口（毫秒） */
#define BUTTON_DEBOUNCE_MS          50

/** @brief 按键轮询间隔（毫秒） */
#define BUTTON_POLL_INTERVAL_MS     20

/* ========================================== USB 模式常量 ============================================= */

#define BUTTON_USB_MODE_MSC  0  /**< 文件存储模式 */
#define BUTTON_USB_MODE_HID  1  /**< 键鼠控制模式 */
#define BUTTON_USB_MODE_NCM  2  /**< 无线网卡模式 */

/* ========================================== 公共 API ============================================= */

/**
 * @brief 初始化按键管理模块
 *
 * 通过事件总线驱动，无需配置参数。
 * 注册 EVENT_USB_MODE_CHANGED 监听刷新模式缓存，
 * 注册 EVENT_USB_SWITCH_REQUEST 请求模式切换。
 *
 * 配置 GPIO 输入、创建轮询任务。
 *
 * @return
 *      - ESP_OK              初始化成功
 *      - ESP_ERR_NO_MEM      资源不足
 */
esp_err_t button_manager_init(void);

#ifdef __cplusplus
}
#endif
