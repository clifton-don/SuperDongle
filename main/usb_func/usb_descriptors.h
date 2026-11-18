/**
 * @file    usb_descriptors.h
 * @brief   USB 单功能描述符定义
 *
 * 为 MSC 和 HID 两种单功能模式提供独立的 USB 配置描述符。
 * esp_tinyusb 默认描述符不支持 HID，必须通过本模块自定义。
 *
 * @date    2026-05-28
 * @author  Mr.Tang
 */

#pragma once

#include <stdint.h>

#include "tinyusb.h"
#include "usb_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================== USB 设备信息 ============================================= */

#define USB_DESC_VID               0x303A               /**< 厂商 ID (Espressif) */
#define USB_DESC_PID_MSC           0x4001               /**< 产品 ID（MSC 模式） */
#define USB_DESC_PID_HID           0x4004               /**< 产品 ID（HID 模式） */
#define USB_DESC_PID_NCM           0x4005               /**< 产品 ID（NCM 模式） */
#define USB_DESC_BCD               0x0100               /**< 设备版本号 */

#define USB_DESC_MANUFACTURER      "Espressif"           /**< 厂商字符串 */
#define USB_DESC_PRODUCT           "SuperDongle"          /**< 产品字符串 */
#define USB_DESC_MSC_INTERFACE     "SuperDongle MSC"      /**< MSC 接口字符串 */
#define USB_DESC_HID_INTERFACE     "SuperDongle HID"      /**< HID 接口字符串 */
#define USB_DESC_NCM_INTERFACE     "SuperDongle NCM"      /**< NCM 接口字符串 */
#define USB_DESC_NCM_MAC           ""                     /**< NCM MAC 字符串（空串由 tinyusb 填充） */

/* ========================================== 字符串描述符索引 ============================================= */

enum {
    USB_STRIDX_LANG         = 0x00,  /**< 语言 ID */
    USB_STRIDX_MANUFACTURER = 0x01,  /**< 厂商索引 */
    USB_STRIDX_PRODUCT      = 0x02,  /**< 产品索引 */
    USB_STRIDX_SERIAL       = 0x03,  /**< 序列号索引 */
    USB_STRIDX_MSC          = 0x04,  /**< MSC 接口字符串索引 */
    USB_STRIDX_NCM          = 0x05,  /**< NCM 接口字符串索引（对齐组件 STRID_NET_INTERFACE） */
    USB_STRIDX_MAC          = 0x06,  /**< NCM MAC 字符串索引（对齐组件 STRID_MAC） */
    USB_STRIDX_HID          = 0x07,  /**< HID 接口字符串索引 */
    USB_STRIDX_COUNT        = 0x08,  /**< 字符串总数 */
};

/* ========================================== 接口编号 ============================================= */

enum {
    ITF_NUM_MSC         = 0,  /**< MSC 接口编号（MSC 模式） */
    ITF_NUM_HID_KB      = 0,  /**< HID 键盘接口编号（HID 模式） */
    ITF_NUM_HID_MOUSE   = 1,  /**< HID 鼠标接口编号（HID 模式） */
    ITF_NUM_NCM_COMM    = 0,  /**< NCM CDC 通信接口编号（NCM 模式） */
    ITF_NUM_NCM_DATA    = 1,  /**< NCM CDC 数据接口编号（NCM 模式） */
    ITF_NUM_COUNT
};

/* ========================================== 端点地址 ============================================= */

enum {
    EP_NUM_MSC_OUT       = 0x01,  /**< MSC BULK OUT 端点 */
    EP_NUM_MSC_IN        = 0x81,  /**< MSC BULK IN 端点 */
    EP_NUM_HID_KB_IN     = 0x81,  /**< HID 键盘 INTR IN 端点 */
    EP_NUM_HID_MOUSE_IN  = 0x82,  /**< HID 鼠标 INTR IN 端点 */
    EP_NUM_NCM_NOTIF     = 0x81,  /**< NCM 通知端点（INTR IN） */
    EP_NUM_NCM_DATA_OUT  = 0x02,  /**< NCM 数据端点（BULK OUT） */
    EP_NUM_NCM_DATA_IN   = 0x82,  /**< NCM 数据端点（BULK IN） */
};

/* ========================================== 端点最大包长 ============================================= */

#define EP_SIZE_MSC_BULK      64    /**< MSC BULK 端点包长（Full-Speed 最大 64） */
#define EP_SIZE_HID_INTR      8     /**< HID INTR 端点包长 */
#define EP_SIZE_NCM_BULK      64    /**< NCM BULK 端点包长（Full-Speed 最大 64） */
#define EP_SIZE_NCM_NOTIF     64    /**< NCM 通知端点包长 */

/* ========================================== 端点轮询间隔 (ms) ============================================= */

#define EP_INTERVAL_HID_KB    10    /**< 键盘轮询间隔 10ms (100Hz，标准键盘速率) */
#define EP_INTERVAL_HID_MOUSE 1     /**< 鼠标轮询间隔 1ms (1000Hz，丝滑移动必需) */

/* ========================================== 公共 API ============================================= */

/**
 * @brief 初始化 USB 描述符（生成基于 MAC 的唯一序列号）
 *
 * 必须在 USB 驱动安装前调用。
 */
void usb_descriptors_init(void);

/**
 * @brief 设置当前模式的描述符
 *
 * 切换 s_active_config_desc 指针到对应模式的配置描述符数组。
 *
 * @param mode USB 运行模式
 */
void usb_descriptors_set_for_mode(usb_manager_mode_t mode);

/**
 * @brief 获取当前描述符配置
 *
 * @return tinyusb_desc_config_t 指针（指向内部静态变量）
 */
const tinyusb_desc_config_t *usb_descriptors_get_config(void);

#ifdef __cplusplus
}
#endif
