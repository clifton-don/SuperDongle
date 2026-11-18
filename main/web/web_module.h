/**
 * @file    web_module.h
 * @brief   Web 服务与配网前端接口
 *
 * 提供 HTTP 服务器初始化和 WiFi 状态查询回调定义，
 * 通过回调函数指针与 wifi_manager 等上层模块解耦。
 *
 * @date    2026-07-26
 * @author  Mr.Tang
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================== USB 模式常量 ========================================== */

#define WEB_USB_MODE_MSC   0  /**< 文件存储模式 */
#define WEB_USB_MODE_HID   1  /**< 键鼠控制模式 */
#define WEB_USB_MODE_NCM   2  /**< 无线网卡模式 */
#define WEB_USB_MODE_COUNT 3  /**< USB 模式总数 */

/** @brief Web 路由描述 */
typedef struct {
    const char      *uri;         /**< URI 路径 */
    httpd_method_t   method;      /**< HTTP 方法（HTTP_GET / HTTP_POST 等） */
    esp_err_t      (*handler)(httpd_req_t *req); /**< 处理函数 */
    bool             is_websocket; /**< 是否为 WebSocket 端点 */
} web_route_t;

/** @brief Web handler 初始化函数类型 */
typedef void (*web_handler_init_fn)(void);

/* ========================================== 注册 API ========================================== */

/**
 * @brief 注册 Web handler 初始化函数
 *
 * 各业务模块通过构造函数在编译期注册自己，
 * web_module_init() 遍历注册表依次调用 init_fn。
 *
 * @param name    模块名称（仅用于日志）
 * @param init_fn 初始化函数指针
 * @return
 *      - ESP_OK          注册成功
 *      - ESP_ERR_NO_MEM  注册表已满
 */
esp_err_t web_module_register_handler(const char *name, web_handler_init_fn init_fn);

/* ========================================== 公共 API ========================================== */

/**
 * @brief 注册一组路由到 Web 服务
 *
 * 可在 web_module_init() 之前或之后调用。
 * - 若尚未启动 HTTP 服务，路由会被缓存，等服务启动后自动注册
 * - 若已启动 HTTP 服务，路由被立即注册
 *
 * @param routes  路由表（静态数组，调用方需保证生命周期）
 * @param count   路由数量
 * @return
 *      - ESP_OK          注册成功
 *      - ESP_ERR_INVALID_ARG  参数无效
 */
esp_err_t web_module_register_routes(const web_route_t *routes, size_t count);

/**
 * @brief URL 解码辅助函数（原地修改）
 *
 * 将 %XX 转义序列和 + 号解码为原始字符。
 *
 * @param str 待解码字符串（原地修改）
 */
void web_url_decode_inplace(char *str);

/**
 * @brief 初始化 Web 配网模块
 *
 * 挂载 SPIFFS 分区（label: "wifi_spiffs"，base_path: "/spiffs"），
 * 启动 HTTP 服务器并注册静态文件路由及各 API 端点。
 *
 * 函数指针依赖通过直接调用各模块 API 实现，无需外部注入。
 *
 * @param http_port HTTP 监听端口，<= 0 时使用默认值 80
 * @return
 *      - ESP_OK              初始化成功（可重复调用）
 *      - ESP_ERR_NO_MEM      内部资源不足
 *      - 其他                SPIFFS / HTTP 服务器相关错误
 */
esp_err_t web_module_init(int http_port);

/**
 * @brief 关闭当前 HID WebSocket 连接（模式切换/桥接停止时调用）
 */
void usb_web_close_hid_ws(void);

/**
 * @brief 获取 Web 服务器句柄
 *
 * @return httpd_handle_t HTTP 服务器句柄
 */
httpd_handle_t web_module_get_server_handle(void);

#ifdef __cplusplus
}
#endif
