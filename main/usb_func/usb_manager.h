/**
 * @file    usb_manager.h
 * @brief   USB 单功能管理模块接口
 *
 * 管理 USB 设备的单一模式运行与切换（MSC 或 HID），
 * 每次仅运行一种功能，切换时完全拆除当前功能再安装新模式。
 * 模式通过 NVS 持久化，上电恢复上次选择。
 *
 * @date    2026-05-28
 * @author  Mr.Tang
 */

#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================== 类型定义 ============================================= */

/** @brief USB 运行模式（互斥，每次仅启用一种） */
typedef enum {
    USB_MODE_MSC_ONLY = 0,  /**< 仅大容量存储 */
    USB_MODE_HID_ONLY = 1,  /**< 仅键鼠 HID */
    USB_MODE_NCM_ONLY = 2,  /**< 仅 USB 网卡（NCM） */
    USB_MODE_COUNT
} usb_manager_mode_t;

/** @brief USB 管理模块配置 */
typedef struct {
    usb_manager_mode_t default_mode;  /**< NVS 无记录时的默认模式 */
} usb_manager_config_t;

/** @brief USB 管理模块默认配置 */
#define USB_MANAGER_DEFAULT_CONFIG()                \
    (usb_manager_config_t) {                        \
        .default_mode = USB_MODE_MSC_ONLY,          \
    }

/* ========================================== USB 管理 API ============================================= */

/**
 * @brief 初始化 USB 管理器
 *
 * 从 NVS 恢复上次模式（或使用 default_mode），安装 USB 驱动并启动对应功能。
 *
 * @param config 配置指针，NULL 为使用 USB_MANAGER_DEFAULT_CONFIG
 * @return
 *      - ESP_OK              初始化成功
 *      - ESP_ERR_NOT_FOUND   SD 卡未插入（MSC 模式需要）
 *      - ESP_ERR_NO_MEM      资源不足
 */
esp_err_t usb_manager_init(const usb_manager_config_t *config);

/**
 * @brief 切换 USB 运行模式
 *
 * 停止当前功能、拆除 USB 驱动、安装新模式、启动新功能、写入 NVS。
 *
 * @param mode 目标模式
 * @return
 *      - ESP_OK              切换成功
 *      - ESP_ERR_INVALID_ARG mode 非法
 *      - ESP_ERR_INVALID_STATE 管理器未初始化
 */
esp_err_t usb_manager_set_mode(usb_manager_mode_t mode);

/**
 * @brief 获取当前 USB 运行模式
 *
 * @return 当前模式枚举值
 */
usb_manager_mode_t usb_manager_get_mode(void);

/**
 * @brief 模式枚举转字符串
 *
 * @param mode 模式枚举
 * @return 字符串字面量（"MSC" / "HID" / "NCM" / "UNKNOWN"）
 */
const char *usb_manager_mode_to_string(usb_manager_mode_t mode);

/**
 * @brief 获取预切换模式
 *
 * 按钮长按 2s 时设 pending_mode，供 web 端 /api/usb/status 提前获知即将切换。
 *
 * @return 预切换模式值（0/1/2），-1 表示无待切换模式
 */
int usb_manager_get_pending_mode(void);

/**
 * @brief 设置预切换模式
 *
 * @param mode 预切换模式值（0/1/2），-1 清除
 */
void usb_manager_set_pending_mode(int mode);

/**
 * @brief 重启当前 USB 功能（不改模式）
 *
 * 拆除后重新安装，用于 USB 异常恢复。
 *
 * @return
 *      - ESP_OK              重启成功
 *      - ESP_ERR_INVALID_STATE 管理器未初始化
 */
esp_err_t usb_manager_restart(void);

/* ========================================== HID 桥接 API ============================================= */

/**
 * @brief 查询 HID 桥接是否运行中
 *
 * @return true=运行中，false=未运行
 */
bool usb_hid_is_active(void);

/**
 * @brief 发送键盘 HID 报告
 *
 * @param modifier 修饰键位掩码（bit0=CTRL, bit1=SHIFT, bit2=ALT, bit3=GUI）
 * @param keycode  6 字节按键码数组，未使用位置填 0
 * @return
 *      - ESP_OK              入队成功
 *      - ESP_ERR_INVALID_STATE HID 未激活
 *      - ESP_ERR_TIMEOUT     队列满
 */
esp_err_t usb_hid_keyboard_send(uint8_t modifier, const uint8_t keycode[6]);

/**
 * @brief 发送鼠标 HID 报告
 *
 * @param buttons 按键状态位掩码（bit0=左键, bit1=右键, bit2=中键）
 * @param x       X 轴相对位移（-127 ~ 127）
 * @param y       Y 轴相对位移（-127 ~ 127）
 * @param wheel   滚轮位移（-127 ~ 127）
 * @return
 *      - ESP_OK              入队成功
 *      - ESP_ERR_INVALID_STATE HID 未激活
 *      - ESP_ERR_TIMEOUT     队列满
 */
esp_err_t usb_hid_mouse_send(uint8_t buttons, int8_t x, int8_t y, int8_t wheel);

/** @brief 桥接停止通知回调（模式切换/重启时调用，用于关闭关联的 WS 等资源） */
typedef void (*usb_manager_bridge_stop_cb_t)(void);

/**
 * @brief 注册桥接停止回调
 * @param cb 回调函数指针，传 NULL 取消注册
 */
void usb_manager_register_bridge_stop_callback(usb_manager_bridge_stop_cb_t cb);

#ifdef __cplusplus
}
#endif
