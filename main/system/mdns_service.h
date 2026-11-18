/**
 * @file    mdns_service.h
 * @brief   mDNS 服务模块接口
 *
 * 独立管理 mDNS 初始化、主机名持久化（NVS）、运行时更新。
 * 默认主机名 "superdongle"，可通过 Web 面板修改。
 *
 * @date    2026-07-24
 * @author  Mr.Tang
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 mDNS 服务
 *
 * 从 NVS 读取已保存的主机名，若无则使用默认值 "superdongle"，
 * 然后初始化 mDNS 并注册 _http._tcp 服务。
 *
 * @return
 *      - ESP_OK          初始化成功（mDNS 初始化失败仅打印错误，不阻塞）
 *      - 其他 esp_err_t  NVS 读取失败
 */
esp_err_t mdns_service_init(void);

/**
 * @brief 获取当前 mDNS 主机名
 * @return 主机名字符串指针（内部静态缓冲区，不可释放）
 */
const char *mdns_service_get_hostname(void);

/**
 * @brief 设置并持久化 mDNS 主机名
 *
 * 保存到 NVS 并立即更新 mDNS（运行时生效，无需重启）。
 *
 * @param hostname 主机名（1~31 字符，仅允许字母、数字、连字符）
 * @return
 *      - ESP_OK          保存成功
 *      - ESP_ERR_INVALID_ARG  参数无效
 *      - 其他 esp_err_t  NVS 写入失败
 */
esp_err_t mdns_service_set_hostname(const char *hostname);

#ifdef __cplusplus
}
#endif
