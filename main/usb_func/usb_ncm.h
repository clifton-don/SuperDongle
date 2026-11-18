/**
 * @file    usb_ncm.h
 * @brief   USB NCM 网卡桥接模块接口
 *
 * 将 WiFi STA 与 USB NCM 桥接，使设备作为 USB 网卡使用。
 * 仅在 USB_MODE_NCM_ONLY 模式下激活，桥接期间 WiFi 协议栈
 * （lwIP）流量被完全拦截转发至 USB 端，Web 服务等均不可用。
 *
 * @date    2026-06-08
 * @author  Mr.Tang
 */

#pragma once

#include "esp_err.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_TINYUSB_NET_MODE_NCM

/**
 * @brief 初始化 NCM 网卡桥接
 *
 * 注册 WiFi STA RX 回调，将 WiFi 收到的数据包转发至 USB NCM；
 * 同时注册 USB 接收回调，将 USB NCM 收到的数据包通过 esp_wifi_internal_tx 发出。
 *
 * @return
 *      - ESP_OK              初始化成功
 *      - ESP_ERR_INVALID_STATE TinyUSB NCM 未配置或 WiFi STA 未初始化
 *      - 其他                具体错误见日志
 */
esp_err_t usb_ncm_init(void);

/**
 * @brief 反初始化 NCM 网卡桥接
 *
 * 取消 WiFi STA RX 回调，恢复 WiFi 协议栈正常收包。
 * 同时取消 USB 桥接，释放相关资源。
 */
void usb_ncm_deinit(void);

#else  /* !CONFIG_TINYUSB_NET_MODE_NCM */

/* Stub 实现，当 NCM 未在 Kconfig 中启用时返回错误 */
static inline esp_err_t usb_ncm_init(void) { return ESP_ERR_NOT_SUPPORTED; }
static inline void usb_ncm_deinit(void) {}

#endif /* CONFIG_TINYUSB_NET_MODE_NCM */

#ifdef __cplusplus
}
#endif
