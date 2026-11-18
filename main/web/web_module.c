/**
 * @file    web_module.c
 * @brief   Web 服务核心：HTTP 服务器 + SPIFFS 静态文件服务
 *
 * 负责：
 * - HTTP 服务器启动/管理（基于 esp_http_server）
 * - SPIFFS 静态文件服务（前端 SPA）
 * - 路由注册 API（各业务模块通过 web_module_register_routes 自主注册）
 *
 * 业务路由已拆分到 web_handlers/ 目录下各独立文件。
 *
 * @date    2026-07-26
 * @author  Mr.Tang
 */

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"

#include "storage_module.h"
#include "usb_manager.h"
#include "web_module.h"

static const char *TAG = "[web_module]";

/* ========================================== 模块级静态变量 ========================================== */

static bool               s_web_inited  = false; /**< Web 模块初始化标志 */
static httpd_handle_t     s_http_server = NULL;  /**< HTTP 服务器句柄 */
static int               s_web_port    = 0;      /**< HTTP 服务端口（默认 80） */

/* ========================================== 路由缓存 ========================================== */

#define MAX_WEB_HANDLER_MODULES 16  /**< 最大 Web 业务模块数（同时也是路由缓存和 Handler 注册表的上限） */

static struct {
    const web_route_t *routes;
    size_t             count;
} s_route_cache[MAX_WEB_HANDLER_MODULES];
static size_t s_route_cache_count = 0;

/* ========================================== Handler 注册表 ========================================== */

static struct {
    const char           *name;
    web_handler_init_fn   init_fn;
} s_registered_handlers[MAX_WEB_HANDLER_MODULES];

static size_t s_registered_handler_count = 0;

/* ========================================== URL 解码辅助 ========================================== */

/**
 * @brief 十六进制字符转整数值
 *
 * @param c 十六进制字符（'0'-'9', 'a'-'f', 'A'-'F'）
 * @return 0-15 有效，-1 非法
 */
static int web_hex_to_int(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    c = (char)tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    return -1;
}

/**
 * @brief 对形如 "%40" 的 URL 编码在原地解码
 *
 * 处理 "%" + 2 位十六进制编码以及 "+" 转空格
 * 用于解码查询字符串中 ssid/password 等参数
 *
 * @param str 待解码字符串（原地修改）
 */
void web_url_decode_inplace(char *str)
{
    if (str == NULL) {
        return;
    }

    char *src = str;
    char *dst = str;

    while (*src != '\0') {
        if (*src == '%' && isxdigit((unsigned char)src[1])
            && isxdigit((unsigned char)src[2])) {
            int hi = web_hex_to_int(src[1]);
            int lo = web_hex_to_int(src[2]);
            if (hi >= 0 && lo >= 0) {
                *dst = (char)((hi << 4) | lo);
                src += 3;
            } else {
                *dst = *src;
                src++;
            }
        } else if (*src == '+') {
            *dst = ' ';
            src++;
        } else {
            *dst = *src;
            src++;
        }
        dst++;
    }

    *dst = '\0';
}

/* ========================================== 静态文件响应辅助 ========================================== */

/**
 * @brief 根据文件扩展名返回对应的 Content-Type
 *
 * @param filename 文件名或路径
 * @return MIME 类型字符串
 */
static const char *web_get_content_type(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (ext == NULL) {
        return "text/plain";
    }
    if (strcmp(ext, ".htm") == 0 || strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".ico") == 0) return "image/x-icon";
    if (strcmp(ext, ".xml") == 0) return "text/xml";
    if (strcmp(ext, ".pdf") == 0) return "application/x-pdf";
    if (strcmp(ext, ".zip") == 0) return "application/x-zip";
    if (strcmp(ext, ".gz") == 0) return "application/x-gzip";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcmp(ext, ".mp4") == 0) return "video/mp4";
    if (strcmp(ext, ".webm") == 0) return "video/webm";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".flac") == 0) return "audio/flac";
    if (strcmp(ext, ".ogg") == 0) return "audio/ogg";
    if (strcmp(ext, ".woff") == 0) return "font/woff";
    if (strcmp(ext, ".woff2") == 0) return "font/woff2";
    if (strcmp(ext, ".ttf") == 0) return "font/ttf";
    if (strcmp(ext, ".eot") == 0) return "application/vnd.ms-fontobject";
    return "text/plain";
}

/**
 * @brief 以分块响应方式发送一个静态文件
 *
 * @param req          HTTP 请求对象
 * @param file_path    文件在 SPIFFS 上的完整路径（如 "/spiffs/index.html"）
 * @param content_type Content-Type 头部
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
static esp_err_t web_module_serve_file(httpd_req_t *req, const char *file_path,
                                       const char *content_type)
{
    FILE *f = fopen(file_path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "open file failed: %s", file_path);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "open file failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, content_type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    char   buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, n) != ESP_OK) {
            fclose(f);
            httpd_resp_sendstr_chunk(req, NULL);
            return ESP_FAIL;
        }
    }

    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/**
 * @brief 核心路由：根据 req->uri 在 SPIFFS 查找并返回静态文件
 *
 * @param req HTTP 请求对象
 * @return ESP_OK（错误通过 HTTP 响应传达）
 */
static esp_err_t serve_static_by_resource(httpd_req_t *req)
{
    char resource_path[PATH_MAX] = {0};
    char spiffs_path[PATH_MAX + 16] = {0};

    const char *uri = req->uri;

    if (uri[strlen(uri) - 1] == '/') {
        strncpy(resource_path, "/index.html", sizeof(resource_path) - 1);
    } else {
        strncpy(resource_path, uri, sizeof(resource_path) - 1);
    }

    if (strstr(resource_path, "../") != NULL) {
        ESP_LOGE(TAG, "path traversal attempt: %s", resource_path);
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "forbidden");
        return ESP_OK;
    }

    snprintf(spiffs_path, sizeof(spiffs_path), "/spiffs%s", resource_path);

    FILE *f = fopen(spiffs_path, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "file not found: %s", spiffs_path);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "file not found");
        return ESP_OK;
    }
    fclose(f);

    const char *content_type = web_get_content_type(resource_path);
    ESP_LOGI(TAG, "serving static file: %s (type: %s)", spiffs_path, content_type);

    return web_module_serve_file(req, spiffs_path, content_type);
}

/**
 * @brief 精确路由 handler（用于高频入口文件如 / /index.html）
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t web_module_file_handler(httpd_req_t *req)
{
    return serve_static_by_resource(req);
}

/**
 * @brief 404 fallback handler：所有未匹配的路由尝试当作静态文件处理
 *
 * @param req   HTTP 请求对象
 * @param error HTTP 错误码（未使用）
 * @return ESP_OK
 */
static esp_err_t web_module_static_handler(httpd_req_t *req, httpd_err_code_t error)
{
    (void)error;
    return serve_static_by_resource(req);
}

/* ========================================== HTTP 服务器启动 ========================================== */

/**
 * @brief 启动 HTTP 服务器并注册全部 URI handler
 *
 * 注册顺序：WiFi API → MSC API → 设置 API → OTA API → 静态文件精确路由 → 404 fallback
 *
 * @return ESP_OK 成功，其他为 httpd_start 错误码
 */
static esp_err_t web_module_start_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers  = 36;
    config.stack_size        = 10240;
    config.max_open_sockets  = 14;

    if (s_web_port > 0) {
        config.server_port = (uint16_t)s_web_port;
    }

    if (s_http_server != NULL) {
        return ESP_OK;
    }

    esp_err_t ret = httpd_start(&s_http_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(ret));
        s_http_server = NULL;
        return ret;
    }

    /* --- 注册各业务模块缓存的路由 --- */
    for (size_t i = 0; i < s_route_cache_count; i++) {
        for (size_t j = 0; j < s_route_cache[i].count; j++) {
            const web_route_t *r = &s_route_cache[i].routes[j];
            httpd_uri_t uri = {
                .uri = r->uri,
                .method = r->method,
                .handler = r->handler,
                .is_websocket = r->is_websocket
            };
            esp_err_t err = httpd_register_uri_handler(s_http_server, &uri);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "route %s register failed: %s",
                         r->uri, esp_err_to_name(err));
            }
        }
    }

    /* --- 精确静态路由（高频入口文件，不走 404 fallback）--- */
    static const httpd_uri_t uri_root = {
        .uri = "/", .method = HTTP_GET, .handler = web_module_file_handler
    };
    httpd_register_uri_handler(s_http_server, &uri_root);

    static const httpd_uri_t uri_index = {
        .uri = "/index.html", .method = HTTP_GET, .handler = web_module_file_handler
    };
    httpd_register_uri_handler(s_http_server, &uri_index);

    /* --- 404 fallback 自动兜底 SPIFFS 静态文件 --- */
    httpd_register_err_handler(s_http_server, HTTPD_404_NOT_FOUND,
                               web_module_static_handler);

    return ESP_OK;
}

/* ========================================== 公共 API ========================================== */

/**
 * @brief 参见 web_module.h
 */
esp_err_t web_module_register_routes(const web_route_t *routes, size_t count)
{
    if (routes == NULL || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_http_server != NULL) {
        /* 服务器已启动，立即注册 */
        for (size_t j = 0; j < count; j++) {
            const web_route_t *r = &routes[j];
            httpd_uri_t uri = {
                .uri = r->uri,
                .method = r->method,
                .handler = r->handler,
                .is_websocket = r->is_websocket
            };
            esp_err_t err = httpd_register_uri_handler(s_http_server, &uri);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "route %s register failed: %s",
                         r->uri, esp_err_to_name(err));
            }
        }
    } else {
        /* 服务器未启动，缓存起来 */
        if (s_route_cache_count >= MAX_WEB_HANDLER_MODULES) {
            ESP_LOGE(TAG, "route cache full (%d)", MAX_WEB_HANDLER_MODULES);
            return ESP_ERR_NO_MEM;
        }
        size_t idx = s_route_cache_count++;
        s_route_cache[idx].routes = routes;
        s_route_cache[idx].count  = count;
    }

    return ESP_OK;
}

/* ========================================== Handler 注册 ========================================== */

/**
 * @brief 参见 web_module.h
 */
esp_err_t web_module_register_handler(const char *name, web_handler_init_fn init_fn)
{
    if (name == NULL || init_fn == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_registered_handler_count >= MAX_WEB_HANDLER_MODULES) {
        ESP_LOGE(TAG, "handler registration full (%d)", MAX_WEB_HANDLER_MODULES);
        return ESP_ERR_NO_MEM;
    }

    s_registered_handlers[s_registered_handler_count].name    = name;
    s_registered_handlers[s_registered_handler_count].init_fn = init_fn;
    s_registered_handler_count++;

    return ESP_OK;
}

/**
 * @brief 参见 web_module.h
 */
esp_err_t web_module_init(int http_port)
{
    s_web_port = http_port;

    if (s_web_inited) {
        return ESP_OK;
    }

    /* 遍历注册表，依次初始化各业务模块 */
    for (size_t i = 0; i < s_registered_handler_count; i++) {
        ESP_LOGI(TAG, "initializing handler: %s", s_registered_handlers[i].name);
        s_registered_handlers[i].init_fn();
    }

    /* 注册桥接停止回调 */
    usb_manager_register_bridge_stop_callback(usb_web_close_hid_ws);

    esp_err_t ret = storage_spiffs_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SPIFFS init failed (%s), web UI file serving may be unavailable",
                 esp_err_to_name(ret));
    }

    ret = web_module_start_server();
    if (ret != ESP_OK) {
        return ret;
    }

    s_web_inited = true;
    return ESP_OK;
}

httpd_handle_t web_module_get_server_handle(void)
{
    return s_http_server;
}
