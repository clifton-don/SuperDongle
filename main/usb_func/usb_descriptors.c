/**
 * @file    usb_descriptors.c
 * @brief   USB 单功能描述符实现
 *
 * 维护 MSC 与 HID 两套独立的静态配置描述符，
 * 通过 tud_descriptor_* 回调函数提供给 TinyUSB 协议栈。
 *
 * @date    2026-05-28
 * @author  Mr.Tang
 */

#include "usb_descriptors.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "sdkconfig.h"
#include "tusb.h"

static const char *TAG = "[usb_desc]";

static usb_manager_mode_t s_current_mode = USB_MODE_MSC_ONLY;  /**< 当前描述符模式 */

/* ========================================== NCM MTU 回退 ============================================= */

#ifndef CFG_TUD_NET_MTU
#define CFG_TUD_NET_MTU   1514  /**< NCM 最大传输单元（以太网标准 1500 + 14 头） */
#endif

/* ========================================== HID 报告描述符 ============================================= */

#if (CONFIG_TINYUSB_HID_COUNT > 0)

static const uint8_t s_hid_report_keyboard[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

static const uint8_t s_hid_report_mouse[] = {
    TUD_HID_REPORT_DESC_MOUSE()
};

#endif /* CONFIG_TINYUSB_HID_COUNT > 0 */

/* ========================================== 配置描述符 : MSC 模式 ========================================= */

enum {
    MSC_TOTAL_LEN = TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN
};

static const uint8_t s_cfg_desc_msc[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, MSC_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, USB_STRIDX_MSC,
                       EP_NUM_MSC_OUT, EP_NUM_MSC_IN, EP_SIZE_MSC_BULK),
};

/* ========================================== 配置描述符 : HID 模式 ========================================= */

#if (CONFIG_TINYUSB_HID_COUNT > 0)

enum {
    HID_TOTAL_LEN = TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN * 2
};

static const uint8_t s_cfg_desc_hid[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, HID_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    TUD_HID_DESCRIPTOR(ITF_NUM_HID_KB, USB_STRIDX_HID,
                       HID_ITF_PROTOCOL_KEYBOARD,
                       sizeof(s_hid_report_keyboard),
                       EP_NUM_HID_KB_IN, EP_SIZE_HID_INTR, EP_INTERVAL_HID_KB),

    TUD_HID_DESCRIPTOR(ITF_NUM_HID_MOUSE, USB_STRIDX_HID,
                       HID_ITF_PROTOCOL_MOUSE,
                       sizeof(s_hid_report_mouse),
                       EP_NUM_HID_MOUSE_IN, EP_SIZE_HID_INTR, EP_INTERVAL_HID_MOUSE),
};

#endif /* CONFIG_TINYUSB_HID_COUNT > 0 */

/* ========================================== 配置描述符 : NCM 模式 ========================================= */

#if CONFIG_TINYUSB_NET_MODE_NCM

enum {
    NCM_TOTAL_LEN = TUD_CONFIG_DESC_LEN + TUD_CDC_NCM_DESC_LEN
};

static const uint8_t s_cfg_desc_ncm[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, NCM_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    TUD_CDC_NCM_DESCRIPTOR(ITF_NUM_NCM_COMM, USB_STRIDX_NCM, USB_STRIDX_MAC,
                           EP_NUM_NCM_NOTIF, EP_SIZE_NCM_NOTIF,
                           EP_NUM_NCM_DATA_OUT, EP_NUM_NCM_DATA_IN,
                           EP_SIZE_NCM_BULK, CFG_TUD_NET_MTU),
};

#endif /* CONFIG_TINYUSB_NET_MODE_NCM */

/* ========================================== 设备描述符 ============================================= */

static tusb_desc_device_t s_device_desc = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = 64,
    .idVendor           = USB_DESC_VID,
    .idProduct          = USB_DESC_PID_MSC,
    .bcdDevice          = USB_DESC_BCD,
    .iManufacturer      = USB_STRIDX_MANUFACTURER,
    .iProduct           = USB_STRIDX_PRODUCT,
    .iSerialNumber      = USB_STRIDX_SERIAL,
    .bNumConfigurations = 0x01,
};

/* ========================================== 运行时的描述符指针 ============================================= */

static const uint8_t *s_active_config_desc = s_cfg_desc_msc;  /**< 当前生效的配置描述符 */

/* ========================================== 字符串描述符 ============================================= */

static char s_serial_str[18]; /**< 序列号 "SD-XXXXXXXXXXXX"，15 字符 + '\0' */

static const char *s_string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 },
    USB_DESC_MANUFACTURER,
    USB_DESC_PRODUCT,
    s_serial_str,
    USB_DESC_MSC_INTERFACE,
    USB_DESC_NCM_INTERFACE,
    USB_DESC_NCM_MAC,
    USB_DESC_HID_INTERFACE,
};

/* ========================================== HID 报告描述符回调 ============================================= */

#if (CONFIG_TINYUSB_HID_COUNT > 0)

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    if (instance == ITF_NUM_HID_KB) {
        return s_hid_report_keyboard;
    }
    if (instance == ITF_NUM_HID_MOUSE) {
        return s_hid_report_mouse;
    }
    return NULL;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

#endif /* CONFIG_TINYUSB_HID_COUNT > 0 */

/* ========================================== 公共 API ============================================= */

/**
 * @brief 参见 usb_descriptors.h
 */
void usb_descriptors_init(void)
{
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);  // 芯片出厂 BASE MAC，永不变化
    snprintf(s_serial_str, sizeof(s_serial_str),
             "SN-%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Device serial: %s", s_serial_str);
}

/**
 * @brief 参见 usb_descriptors.h
 */
void usb_descriptors_set_for_mode(usb_manager_mode_t mode)
{
    s_current_mode = mode;

    switch (mode) {
        case USB_MODE_MSC_ONLY:
            s_active_config_desc  = s_cfg_desc_msc;
            s_device_desc.idProduct = USB_DESC_PID_MSC;
            break;
#if (CONFIG_TINYUSB_HID_COUNT > 0)
        case USB_MODE_HID_ONLY:
            s_active_config_desc  = s_cfg_desc_hid;
            s_device_desc.idProduct = USB_DESC_PID_HID;
            break;
#endif
#if CONFIG_TINYUSB_NET_MODE_NCM
        case USB_MODE_NCM_ONLY:
            s_active_config_desc  = s_cfg_desc_ncm;
            s_device_desc.idProduct = USB_DESC_PID_NCM;
            break;
#endif
        default:
            ESP_LOGW(TAG, "Unsupported mode %d, falling back to MSC descriptors", (int)mode);
            s_active_config_desc  = s_cfg_desc_msc;
            s_device_desc.idProduct = USB_DESC_PID_MSC;
            break;
    }

    ESP_LOGI(TAG, "Mode set to %d, PID=0x%04X", (int)mode, s_device_desc.idProduct);
}

/**
 * @brief 参见 usb_descriptors.h
 */
const tinyusb_desc_config_t *usb_descriptors_get_config(void)
{
    static tinyusb_desc_config_t desc_cfg;

    desc_cfg.device            = &s_device_desc;
    desc_cfg.string            = s_string_desc_arr;
    desc_cfg.string_count      = USB_STRIDX_COUNT;
    desc_cfg.full_speed_config = s_active_config_desc;
    desc_cfg.high_speed_config = NULL;
    desc_cfg.qualifier         = NULL;

    return &desc_cfg;
}
