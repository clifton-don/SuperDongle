/**
 * @file    usb_msc.h
 * @brief   USB 大容量存储设备接口
 *
 * 基于 TinyUSB MSC 实现，与 storage_module 中的 SD 卡 FATFS 配合，
 * 将 SD 卡通过 USB 暴露为计算机可识别的 U 盘设备。
 *
 * @date    2026-05-19
 * @author  Mr.Tang
 */

#pragma once

#include "esp_err.h"
#include "tinyusb_msc.h"

/* ========================================== 公共 API ============================================= */

/**
 * @brief 初始化 USB MSC 设备
 *
 * 安装 TinyUSB MSC 驱动、关联 SD 卡存储、安装 TinyUSB 底层驱动。
 *
 * @return
 *      - ESP_OK      初始化成功
 *      - ESP_FAIL    SD 卡未就绪
 *      - 其他        TinyUSB 驱动安装失败
 */
esp_err_t usb_msc_init(void);

/**
 * @brief 软重启 USB MSC 设备
 *
 * 卸载并重新安装 TinyUSB 驱动，用于上位机软重启，或处理 USB 枚举异常后的恢复。
 *
 * @return
 *      - ESP_OK      重启成功
 *      - 其他        tinyusb_driver_install 错误码
 */
esp_err_t usb_msc_restart(void);