/**
 * @file    wifi_manager.h
 * @brief   WiFi 管理层（状态机）接口定义
 *
 * 提供 WiFi 连接管理、自动重连、AP 配网等功能的配置与初始化入口。
 * 通过回调函数与上层应用（LED、Web 等）解耦。
 *
 * @date    2026-05-19
 * @author  Mr.Tang
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/* ========================================== WiFi 状态类型 ============================================= */

/**
 * @brief Web 配网模块关注的 WiFi 状态视图
 */
typedef enum {
    WEB_WIFI_STATUS_STATE_IDLE = 0,
    WEB_WIFI_STATUS_STATE_CONNECTING,
    WEB_WIFI_STATUS_STATE_CONNECTED,
    WEB_WIFI_STATUS_STATE_FAILED,
} web_wifi_status_state_t;

/** @brief WiFi 状态快照，供前端页面展示 */
typedef struct web_wifi_status_s {
    web_wifi_status_state_t state;
    bool connected;
    char ssid[32];
    char ip[16];
    int8_t rssi;
    char mode[8];
    char hostname[32];
} web_wifi_status_t;

/** @brief Web 端展示用的"已保存 WiFi"精简信息 */
typedef struct web_saved_wifi_info_s {
    char ssid[32];
} web_saved_wifi_info_t;

/** @brief Web 端展示用的"扫描结果"精简信息 */
typedef struct web_scan_result_s {
    char   ssid[32];
    int8_t rssi;
    uint8_t authmode;
} web_scan_result_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================== 配置宏 ============================================= */

/**
 * @brief WiFi 管理状态机单步运行周期（ms）
 *
 * 管理任务按此间隔周期性驱动内部状态机。间隔过小浪费 CPU，过大降低重连响应速度。
 */
#define WIFI_MANAGE_STEP_INTERVAL_MS 1000

/* ========================================== 类型定义 ============================================= */

/**
 * @brief WiFi 管理层抽象连接状态
 *
 * 向应用层暴露「是否连上路由器」的结果，不暴露底层驱动细节。
 */
typedef enum {
    WIFI_MANAGE_STATE_DISCONNECTED = 0, /**< 已断开，无可用连接 */
    WIFI_MANAGE_STATE_CONNECTED,         /**< 已成功连接并获得 IP */
    WIFI_MANAGE_STATE_CONNECT_FAILED,    /**< 本轮全部候选 WiFi 连接失败 */
} wifi_manage_state_t;

/**
 * @brief WiFi 管理模块配置
 *
 * 初始化时读取一次，由管理模块内部持有副本。
 */
typedef struct {
    int              max_retry_count;       /**< 单个 AP 最多重试次数，<=0 仅尝试一次 */
    int              reconnect_interval_ms; /**< 全轮失败后等待重试间隔（ms），<0 关闭自动重连 */
    char             ap_ssid[32];           /**< 配网 AP SSID，最长 31 字符 */
    char             ap_password[64];       /**< 配网 AP 密码，8~63 字符 */
    char             ap_ip[16];             /**< 配网 AP 静态 IP，如 "192.168.4.1" */
    int              save_wifi_count;       /**< 最多保存的 WiFi 条目数，<=0 强制为 1 */
    int              web_port;              /**< Web 配网 HTTP 端口，典型 80 */
} wifi_manage_config_t;

/**
 * @brief WiFi 管理模块默认配置
 *
 * 可在默认基础上仅修改关注字段：
 * @code
 * wifi_manage_config_t cfg = WIFI_MANAGE_DEFAULT_CONFIG();
 * strcpy(cfg.ap_ssid, "MyAP");
 * wifi_manage_init(&cfg);
 * @endcode
 */
#define WIFI_MANAGE_DEFAULT_CONFIG()                       \
    (wifi_manage_config_t) {                                \
        .max_retry_count       = 5,                        \
        .reconnect_interval_ms = 10000,                    \
        .ap_ssid               = "SuperDongle",            \
        .ap_password           = "12345678",               \
        .ap_ip                 = "192.168.5.1",            \
        .save_wifi_count       = 5,                        \
        .web_port              = 80,                       \
    }

/* ========================================== 公共 API ============================================= */

/**
 * @brief 初始化 WiFi 管理模块
 *
 * 初始化内部 WiFi / 存储 / Web 配网子模块，创建管理任务并启动状态机，
 * 根据配置启动 STA + AP 模式。
 *
 * @param config 配置指针，NULL 则使用 WIFI_MANAGE_DEFAULT_CONFIG
 * @return
 *      - ESP_OK         初始化成功
 *      - 其他 esp_err_t  子模块返回的错误码
 */
esp_err_t wifi_manage_init(const wifi_manage_config_t *config);

/**
 * @brief 下一次 STA 断开时跳过 AP 开启
 *
 * 用于从 NCM 退出时断开 WiFi 重连不应打开 AP。
 * 该标志仅生效一次，在 STA 断开事件处理中自动清除。
 */
void wifi_manage_skip_ap_once(void);

/* ========================================== 给 Web 模块的回调函数 ============================================= */

/**
 * @brief 获取 Web 配网端口
 * @return 端口号
 */
int wifi_manage_get_web_port(void);

/**
 * @brief 以下几种回调由 wifi_manager 提供给 web_module，main.c 负责转交
 */
esp_err_t wifi_manage_get_web_status(web_wifi_status_t *out);
esp_err_t wifi_manage_get_web_saved_list(web_saved_wifi_info_t *list, size_t *inout_cnt);
esp_err_t wifi_manage_scan_web(web_scan_result_t *list, size_t *inout_cnt);
esp_err_t wifi_manage_delete_web_saved(const char *ssid);
esp_err_t wifi_manage_connect_web_saved(const char *ssid);
esp_err_t wifi_manage_connect_web_form(const char *ssid, const char *password);

#ifdef __cplusplus
}
#endif