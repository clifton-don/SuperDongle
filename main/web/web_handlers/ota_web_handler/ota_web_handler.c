/**
 * @file    ota_web_handler.c
 * @brief   OTA 升级 Web handler 实现
 *
 * 提供 OTA 升级相关的 REST API 端点：
 *   - /api/ota/status       GET    查询 OTA 状态
 *   - /api/ota/check        POST   触发 OTA 更新检查
 *   - /api/ota/update       POST   启动 OTA 更新
 *   - /api/ota/cancel       POST   取消 OTA 更新
 *   - /api/ota/config       GET    获取 OTA version_url
 *   - /api/ota/config/set   POST   设置 OTA version_url
 *
 * @date    2026-07-26
 * @author  Mr.Tang
 */

#include "web_module.h"

#include "http_ota_manager.h"
#include "event_bus.h"
#include "event_defs.h"

#include "esp_log.h"
#include "esp_system.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "ota_web";

/* ========================================== JSON 辅助 ============================================= */

/**
 * @brief 对 JSON 字符串值做转义（", \, \n, \r, \t）
 *
 * @param dst      目标缓冲区
 * @param dst_size 目标缓冲区大小
 * @param src      源字符串（可为 NULL）
 * @return 写入的字符数（不含 '\0'）
 */
static int json_escape(char *dst, size_t dst_size, const char *src)
{
    if (dst_size == 0) return 0;
    if (src == NULL) {
        dst[0] = '\0';
        return 0;
    }
    size_t pos = 0;
    while (*src && pos + 2 < dst_size) {
        switch (*src) {
        case '\n': dst[pos++] = '\\'; dst[pos++] = 'n'; break;
        case '\r': dst[pos++] = '\\'; dst[pos++] = 'r'; break;
        case '\t': dst[pos++] = '\\'; dst[pos++] = 't'; break;
        case '\\': dst[pos++] = '\\'; dst[pos++] = '\\'; break;
        case '"':  dst[pos++] = '\\'; dst[pos++] = '"'; break;
        default:   dst[pos++] = *src; break;
        }
        src++;
    }
    dst[pos] = '\0';
    return (int)pos;
}

/* ========================================== OTA 状态 API ============================================= */

/**
 * @brief GET /api/ota/status
 *
 * 返回 OTA 升级进度和状态快照。检测到错误时通知 LED 模块。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t ota_web_handler_status_handler(httpd_req_t *req)
{
    if (http_ota_manager_has_error()) {
        event_bus_post(EVENT_OTA_ERROR, NULL, 0);
    }

    char desc_esc[OTA_DESC_MAX * 2];
    char msg_esc[128];
    json_escape(desc_esc, sizeof(desc_esc), http_ota_manager_get_update_desc());
    json_escape(msg_esc, sizeof(msg_esc), http_ota_manager_get_status_msg());

    char resp[2048];
    int  len = snprintf(resp, sizeof(resp),
        "{\"current_version\":\"%s\",\"update_available\":%s,"
        "\"new_version\":\"%s\",\"description\":\"%s\","
        "\"busy\":%s,\"progress\":%d,\"has_firmware\":%s,"
        "\"has_spiffs\":%s,\"error\":%s,\"status_msg\":\"%s\"}",
        http_ota_manager_get_current_version(),
        http_ota_manager_is_update_available() ? "true" : "false",
        http_ota_manager_get_new_version() ? http_ota_manager_get_new_version() : "",
        desc_esc,
        http_ota_manager_is_busy() ? "true" : "false",
        http_ota_manager_get_progress(),
        http_ota_manager_has_firmware() ? "true" : "false",
        http_ota_manager_has_spiffs() ? "true" : "false",
        http_ota_manager_has_error() ? "true" : "false",
        msg_esc
    );
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, len);
    return ESP_OK;
}

/* ========================================== OTA 检查 API ============================================= */

/**
 * @brief POST /api/ota/check
 *
 * 触发 OTA 更新检查。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t ota_web_handler_check_handler(httpd_req_t *req)
{
    if (http_ota_manager_is_busy()) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"busy\"}", 27);
        return ESP_OK;
    }
    esp_err_t ret = http_ota_manager_check();
    httpd_resp_set_type(req, "application/json");
    if (ret == ESP_OK) {
        char resp[128];
        int  len = snprintf(resp, sizeof(resp),
            "{\"ok\":true,\"update_available\":%s}",
            http_ota_manager_is_update_available() ? "true" : "false");
        httpd_resp_send(req, resp, len);
    } else {
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"check_failed\"}", 35);
    }
    return ESP_OK;
}

/* ========================================== OTA 更新 API ============================================= */

/**
 * @brief POST /api/ota/update
 *
 * 启动 OTA 更新流程。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t ota_web_handler_update_handler(httpd_req_t *req)
{
    if (http_ota_manager_is_busy()) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"busy\"}", 27);
        return ESP_OK;
    }
    if (!http_ota_manager_is_update_available()) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"no_update\"}", 29);
        return ESP_OK;
    }

    if (http_ota_manager_start_update() != ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"busy\"}", 27);
        return ESP_OK;
    }

    event_bus_post(EVENT_OTA_STARTED, NULL, 0);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true,\"status\":\"updating\"}", 31);
    return ESP_OK;
}

/* ========================================== OTA 取消 API ============================================= */

/**
 * @brief POST /api/ota/cancel
 *
 * 取消正在进行的 OTA 更新。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t ota_web_handler_cancel_handler(httpd_req_t *req)
{
    http_ota_manager_cancel();
    event_bus_post(EVENT_OTA_ERROR, NULL, 0);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", 9);
    return ESP_OK;
}

/* ========================================== OTA 配置 API ============================================= */

/**
 * @brief GET /api/ota/config
 *
 * 返回当前生效的 OTA version_url（优先 NVS 自定义值，否则为编译期默认值）。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t ota_web_handler_config_get_handler(httpd_req_t *req)
{
    char   url[512];
    char   json_buf[580];
    int    len;

    http_ota_manager_get_version_url(url, sizeof(url));

    bool is_default = (strcmp(url, HTTP_OTA_MANAGER_VERSION_URL) == 0);
    len = snprintf(json_buf, sizeof(json_buf),
                   "{\"version_url\":\"%s\",\"is_default\":%s}",
                   url, is_default ? "true" : "false");
    if (len < 0 || len >= (int)sizeof(json_buf)) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_buf, len);
    return ESP_OK;
}

/**
 * @brief POST /api/ota/config/set
 *
 * Body: {"version_url": "http://..."}
 * 写入 NVS，下次 OTA 自动检查生效。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t ota_web_handler_config_post_handler(httpd_req_t *req)
{
    int  total_len = req->content_len;
    if (total_len <= 0 || total_len > 600) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "Invalid body length");
        return ESP_FAIL;
    }

    char *body = malloc(total_len + 1);
    if (!body) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int ret = httpd_req_recv(req, body, total_len);
    if (ret <= 0) {
        free(body);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    body[ret] = '\0';

    /* Simple JSON parse: find "version_url" value */
    char *key   = strstr(body, "\"version_url\"");
    char *start = NULL;
    char *end   = NULL;
    if (key) {
        start = strchr(key + 13, '\"');  /* skip "version_url" */
        if (start) {
            start++;  /* move past the opening quote */
            end = strchr(start, '\"');
        }
    }

    esp_err_t result;
    if (start && end && end > start) {
        *end = '\0';
        result = http_ota_manager_set_version_url(start);
    } else if (strstr(body, "\"version_url\"")) {
        /* Key exists but value is empty → clear custom setting */
        result = http_ota_manager_set_version_url("");
    } else {
        free(body);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "Missing version_url field");
        return ESP_FAIL;
    }

    free(body);

    if (result != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Failed to save config");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true,\"rebooting\":true}", 28);

    /* Reboot after 1 second so the response can reach the client */
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}

/* ========================================== 路由表 ============================================= */

static const web_route_t s_ota_routes[] = {
    {"/api/ota/status",     HTTP_GET,  ota_web_handler_status_handler,      false},
    {"/api/ota/check",      HTTP_POST, ota_web_handler_check_handler,       false},
    {"/api/ota/update",     HTTP_POST, ota_web_handler_update_handler,      false},
    {"/api/ota/cancel",     HTTP_POST, ota_web_handler_cancel_handler,      false},
    {"/api/ota/config",     HTTP_GET,  ota_web_handler_config_get_handler,  false},
    {"/api/ota/config/set", HTTP_POST, ota_web_handler_config_post_handler, false},
};

/* ========================================== 公共 API ============================================= */

/**
 * @brief 注册 OTA 路由到 Web 服务
 */
void ota_web_handler_init(void)
{
    esp_err_t ret = web_module_register_routes(
        s_ota_routes,
        sizeof(s_ota_routes) / sizeof(s_ota_routes[0])
    );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register routes failed: %s", esp_err_to_name(ret));
    }
}

/**
 * @brief 编译期自动注册本模块初始化函数到 Web 模块
 */
static void __attribute__((constructor, used)) ota_web_handler_auto_init(void)
{
    web_module_register_handler("ota", ota_web_handler_init);
}
