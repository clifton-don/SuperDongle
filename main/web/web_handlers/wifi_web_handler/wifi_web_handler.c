/**
 * @file    wifi_web_handler.c
 * @brief   WiFi Web handler — 配网与 WiFi API HTTP 处理函数
 *
 * 提供 WiFi 状态查询、扫描、连接、已保存列表管理等的 REST API handler。
 *
 * @date    2026-07-26
 * @author  Mr.Tang
 */

#include "web_module.h"

#include "wifi_manager.h"

#include "event_bus.h"
#include "event_defs.h"

#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_http_server.h"


/* ========================================== Handler 实现 ============================================= */

/**
 * @brief GET /api/wifi/status
 *
 * 返回当前 WiFi 状态。若未配置回调则返回占位数据。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t wifi_status_get_handler(httpd_req_t *req)
{
    web_wifi_status_t status = {0};
    char              json[192];

    if (wifi_manage_get_web_status(&status) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "status query failed");
        return ESP_OK;
    }

    int len = snprintf(json,
                       sizeof(json),
                       "{\"connected\":%s,\"state\":%d,\"ssid\":\"%s\","
                       "\"ip\":\"%s\",\"rssi\":%d,\"mode\":\"%s\",\"hostname\":\"%s\"}",
                       status.connected ? "true" : "false",
                       (int)status.state,
                       status.ssid,
                       status.ip,
                       (int)status.rssi,
                       status.mode,
                       status.hostname);

    if (len < 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "format json failed");
        return ESP_OK;
    }
    if (len >= (int)sizeof(json)) {
        len = (int)sizeof(json) - 1;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, json, len);
    return ESP_OK;
}

/**
 * @brief GET /api/wifi/saved
 *
 * 返回 NVS 中已保存的 WiFi 列表。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t wifi_saved_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    size_t    cnt = 0;
    esp_err_t ret = wifi_manage_get_web_saved_list(NULL, &cnt);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "load saved wifi failed");
        return ESP_OK;
    }

    if (cnt == 0) {
        static const char *EMPTY_JSON = "{\"items\":[]}";
        httpd_resp_send(req, EMPTY_JSON, strlen(EMPTY_JSON));
        return ESP_OK;
    }

    web_saved_wifi_info_t *list = (web_saved_wifi_info_t *)heap_caps_malloc(
        cnt * sizeof(web_saved_wifi_info_t), MALLOC_CAP_SPIRAM);
    if (list == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
        return ESP_OK;
    }

    size_t cap = cnt;
    ret = wifi_manage_get_web_saved_list(list, &cap);
    if (ret != ESP_OK) {
        free(list);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "load saved wifi failed");
        return ESP_OK;
    }

    if (cap > cnt) {
        cap = cnt;
    }

    char   json[512];
    size_t offset = 0;

    offset += (size_t)snprintf(json + offset, sizeof(json) - offset, "{\"items\":[");

    for (size_t i = 0; i < cap && offset < sizeof(json); i++) {
        const char *comma = (i == 0) ? "" : ",";
        offset += (size_t)snprintf(json + offset,
                                   sizeof(json) - offset,
                                   "%s{\"index\":%u,\"ssid\":\"%s\"}",
                                   comma,
                                   (unsigned)i,
                                   list[i].ssid);
    }

    free(list);

    if (offset >= sizeof(json)) {
        const char *FALLBACK = "{\"items\":[]}";
        httpd_resp_send(req, FALLBACK, strlen(FALLBACK));
        return ESP_OK;
    }

    offset += (size_t)snprintf(json + offset, sizeof(json) - offset, "]}");

    httpd_resp_send(req, json, (int)offset);
    return ESP_OK;
}

/**
 * @brief GET /api/wifi/scan
 *
 * 触发一次 WiFi 扫描并返回附近 AP 列表。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t wifi_scan_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    enum { WEB_MAX_SCAN_RESULT = 32 };
    web_scan_result_t *list = (web_scan_result_t *)heap_caps_malloc(
        WEB_MAX_SCAN_RESULT * sizeof(web_scan_result_t), MALLOC_CAP_SPIRAM);
    if (list == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
        return ESP_OK;
    }

    size_t    cnt = WEB_MAX_SCAN_RESULT;
    esp_err_t ret = wifi_manage_scan_web(list, &cnt);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "scan failed");
        free(list);
        return ESP_OK;
    }

    if (cnt == 0) {
        static const char *EMPTY_JSON = "{\"items\":[]}";
        httpd_resp_send(req, EMPTY_JSON, strlen(EMPTY_JSON));
        free(list);
        return ESP_OK;
    }

    const size_t json_buf_size = 3072;
    char *json = (char *)heap_caps_malloc(json_buf_size, MALLOC_CAP_SPIRAM);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
        free(list);
        return ESP_OK;
    }

    size_t offset = 0;

    offset += (size_t)snprintf(json + offset, json_buf_size - offset, "{\"items\":[");

    for (size_t i = 0; i < cnt && offset < json_buf_size; i++) {
        const char *comma = (i == 0) ? "" : ",";
        offset += (size_t)snprintf(json + offset,
                                   json_buf_size - offset,
                                   "%s{\"index\":%u,\"ssid\":\"%s\",\"rssi\":%d,\"auth\":%u}",
                                   comma,
                                   (unsigned)i,
                                   list[i].ssid,
                                   (int)list[i].rssi,
                                   (unsigned)list[i].authmode);
    }

    if (offset >= json_buf_size) {
        const char *FALLBACK = "{\"items\":[]}";
        httpd_resp_send(req, FALLBACK, strlen(FALLBACK));
        free(list);
        free(json);
        return ESP_OK;
    }

    offset += (size_t)snprintf(json + offset, json_buf_size - offset, "]}");

    httpd_resp_send(req, json, (int)offset);
    free(json);
    free(list);
    return ESP_OK;
}

/**
 * @brief POST /api/wifi/connect
 *
 * 从查询字符串读取 ssid/password，调用上层 connect_cb 发起连接。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t wifi_connect_handler(httpd_req_t *req)
{
    char query[256]   = {0};
    char ssid[128]    = {0};
    char password[64] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing query");
        return ESP_OK;
    }

    if (httpd_query_key_value(query, "ssid", ssid, sizeof(ssid)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing ssid");
        return ESP_OK;
    }
    (void)httpd_query_key_value(query, "password", password, sizeof(password)); // open AP 时 password 可选

    web_url_decode_inplace(ssid);
    web_url_decode_inplace(password);

    if (ssid[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing ssid");
        return ESP_OK;
    }

    const char *pwd_arg = (password[0] == '\0') ? NULL : password;

    event_bus_post(EVENT_WIFI_CONNECTING, NULL, 0);

    esp_err_t ret = wifi_manage_connect_web_form(ssid, pwd_arg);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "connect failed");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", strlen("{\"ok\":true}"));
    return ESP_OK;
}

/**
 * @brief POST /api/wifi/saved/delete
 *
 * 按 SSID 从 NVS 删除一条已保存 WiFi。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t wifi_saved_delete_handler(httpd_req_t *req)
{
    char query[128] = {0};
    char ssid[64]   = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing query");
        return ESP_OK;
    }

    if (httpd_query_key_value(query, "ssid", ssid, sizeof(ssid)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing ssid");
        return ESP_OK;
    }

    web_url_decode_inplace(ssid);

    if (ssid[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing ssid");
        return ESP_OK;
    }

    esp_err_t ret = wifi_manage_delete_web_saved(ssid);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "delete failed");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", strlen("{\"ok\":true}"));
    return ESP_OK;
}

/**
 * @brief POST /api/wifi/saved/connect
 *
 * 按 SSID 触发连接已保存的 WiFi。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t wifi_saved_connect_handler(httpd_req_t *req)
{
    char query[128] = {0};
    char ssid[64]   = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing query");
        return ESP_OK;
    }

    if (httpd_query_key_value(query, "ssid", ssid, sizeof(ssid)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing ssid");
        return ESP_OK;
    }

    web_url_decode_inplace(ssid);

    if (ssid[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing ssid");
        return ESP_OK;
    }

    event_bus_post(EVENT_WIFI_CONNECTING, NULL, 0);

    esp_err_t ret = wifi_manage_connect_web_saved(ssid);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "connect failed");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", strlen("{\"ok\":true}"));
    return ESP_OK;
}

/* ========================================== 路由表 ============================================= */

static const web_route_t s_wifi_routes[] = {
    {
        .uri          = "/api/wifi/status",
        .method       = HTTP_GET,
        .handler      = wifi_status_get_handler,
        .is_websocket = false,
    },
    {
        .uri          = "/api/wifi/saved",
        .method       = HTTP_GET,
        .handler      = wifi_saved_get_handler,
        .is_websocket = false,
    },
    {
        .uri          = "/api/wifi/scan",
        .method       = HTTP_GET,
        .handler      = wifi_scan_get_handler,
        .is_websocket = false,
    },
    {
        .uri          = "/api/wifi/connect",
        .method       = HTTP_POST,
        .handler      = wifi_connect_handler,
        .is_websocket = false,
    },
    {
        .uri          = "/api/wifi/saved/delete",
        .method       = HTTP_POST,
        .handler      = wifi_saved_delete_handler,
        .is_websocket = false,
    },
    {
        .uri          = "/api/wifi/saved/connect",
        .method       = HTTP_POST,
        .handler      = wifi_saved_connect_handler,
        .is_websocket = false,
    },
};

/* ========================================== 公共 API ============================================= */

void wifi_web_handler_init(void)
{
    web_module_register_routes(s_wifi_routes,
                               sizeof(s_wifi_routes) / sizeof(s_wifi_routes[0]));
}

/**
 * @brief 编译期自动注册本模块初始化函数到 Web 模块
 */
static void __attribute__((constructor, used)) wifi_web_handler_auto_init(void)
{
    web_module_register_handler("wifi", wifi_web_handler_init);
}
