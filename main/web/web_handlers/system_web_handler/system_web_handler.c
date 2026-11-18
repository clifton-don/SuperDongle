/**
 * @file    system_web_handler.c
 * @brief   系统设置 Web handler 实现
 *
 * 提供 LED 控制、mDNS 主机名保存、恢复出厂设置、设备重启等 API。
 *
 * @date    2026-07-26
 * @author  Mr.Tang
 */

#include "system_web_handler.h"
#include "web_module.h"

#include "mdns_service.h"
#include "led_indicator.h"
#include "wifi_storage.h"
#include "event_bus.h"
#include "event_defs.h"

#include <ctype.h>
#include <string.h>

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "system_web";

/* ========================================== 全局设置 API ============================================= */

/**
 * @brief GET /api/settings/led/state
 *
 * 从 NVS 读取 LED 开关状态并返回。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t web_module_led_state_handler(httpd_req_t *req)
{
    nvs_handle_t nvs_h;
    bool enabled = true;
    if (nvs_open("superdongle", NVS_READONLY, &nvs_h) == ESP_OK) {
        uint8_t val = 1;
        nvs_get_u8(nvs_h, "led_enabled", &val);
        nvs_close(nvs_h);
        enabled = (val != 0);
    }
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"enabled\":%s}", enabled ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

/**
 * @brief POST /api/settings/led/save
 *
 * 将 LED 开关状态写入 NVS 并通知 led_indicator 模块。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t web_module_led_save_handler(httpd_req_t *req)
{
    char buf[64] = {0};
    int  ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad request");
        return ESP_OK;
    }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    cJSON *obj = cJSON_GetObjectItem(root, "enabled");
    if (!obj || !cJSON_IsBool(obj)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing enabled field");
        return ESP_OK;
    }

    bool enabled = cJSON_IsTrue(obj);

    nvs_handle_t nvs_h;
    esp_err_t    nvs_ret = nvs_open("superdongle", NVS_READWRITE, &nvs_h);
    if (nvs_ret == ESP_OK) {
        nvs_ret = nvs_set_u8(nvs_h, "led_enabled", enabled ? 1 : 0);
        if (nvs_ret == ESP_OK) {
            nvs_ret = nvs_commit(nvs_h);
            if (nvs_ret == ESP_OK) {
                ESP_LOGI(TAG, "LED enabled=%d saved to NVS", enabled);
            } else {
                ESP_LOGE(TAG, "Failed to commit LED state to NVS: %s",
                         esp_err_to_name(nvs_ret));
            }
        } else {
            ESP_LOGE(TAG, "Failed to set LED state in NVS: %s",
                     esp_err_to_name(nvs_ret));
        }
        nvs_close(nvs_h);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for LED settings: %s",
                 esp_err_to_name(nvs_ret));
    }

    event_bus_post(EVENT_LED_SET_ENABLED, &enabled, sizeof(enabled));
    ESP_LOGI(TAG, "LED %s", enabled ? "turned ON" : "turned OFF");

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", strlen("{\"ok\":true}"));
    return ESP_OK;
}

/**
 * @brief POST /api/settings/mdns/save
 *
 * 将 mDNS 主机名写入 NVS。校验长度 1-31 字符，仅允许字母数字和连字符。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t web_module_mdns_save_handler(httpd_req_t *req)
{
    char buf[128] = {0};
    int  ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad request");
        return ESP_OK;
    }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        ESP_LOGE(TAG, "[mDNS save] invalid JSON: %s", buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    cJSON *host_obj = cJSON_GetObjectItem(root, "hostname");
    if (!host_obj || !cJSON_IsString(host_obj) || host_obj->valuestring[0] == '\0') {
        ESP_LOGE(TAG, "[mDNS save] missing or empty hostname field");
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing hostname");
        return ESP_OK;
    }

    const char *hostname = host_obj->valuestring;
    int         len      = strlen(hostname);
    if (len < 1 || len > 31) {
        ESP_LOGE(TAG, "[mDNS save] invalid hostname length: %d", len);
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Hostname must be 1-31 chars");
        return ESP_OK;
    }
    for (int i = 0; i < len; i++) {
        if (!isalnum((unsigned char)hostname[i]) && hostname[i] != '-') {
            ESP_LOGE(TAG, "[mDNS save] invalid char '%c' in hostname: %s",
                     hostname[i], hostname);
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid characters in hostname");
            return ESP_OK;
        }
    }

    ret = mdns_service_set_hostname(hostname);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[mDNS save] mdns_service_set_hostname failed: %s", esp_err_to_name(ret));
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Save failed");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "[mDNS save] hostname saved: %s.local", hostname);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", strlen("{\"ok\":true}"));

    /* 延迟 500ms 确保响应发送到客户端，然后重启生效 */
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

/* ========================================== 系统管理 API ============================================= */

/**
 * @brief POST /api/system/factory_reset
 *
 * 恢复出厂设置：清除所有 NVS 配置并重启。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t web_module_system_factory_reset_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true,\"resetting\":true}", 27);

    // 延迟一小会儿，让响应先发送出去
    vTaskDelay(pdMS_TO_TICKS(500));

    // 擦除所有 NVS 分区
    ESP_LOGI(TAG, "Performing factory reset...");
    nvs_flash_erase();

    // 重启设备
    ESP_LOGI(TAG, "Rebooting...");
    esp_restart();

    return ESP_OK;
}

/**
 * @brief POST /api/system/reboot
 *
 * 立即重启设备。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t web_module_system_reboot_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true,\"rebooting\":true}", 28);
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

/* ========================================== 路由表 ============================================= */

static const web_route_t s_system_routes[] = {
    {"/api/settings/mdns/save",     HTTP_POST, web_module_mdns_save_handler,         false},
    {"/api/settings/led/state",     HTTP_GET,  web_module_led_state_handler,         false},
    {"/api/settings/led/save",      HTTP_POST, web_module_led_save_handler,          false},
    {"/api/system/factory_reset",   HTTP_POST, web_module_system_factory_reset_handler, false},
    {"/api/system/reboot",          HTTP_POST, web_module_system_reboot_handler,     false},
};

/* ========================================== 公共 API ============================================= */

void system_web_handler_init(void)
{
    web_module_register_routes(s_system_routes,
                               sizeof(s_system_routes) / sizeof(s_system_routes[0]));
    ESP_LOGI(TAG, "system web handlers registered (%d routes)",
             sizeof(s_system_routes) / sizeof(s_system_routes[0]));
}

/**
 * @brief 编译期自动注册本模块初始化函数到 Web 模块
 */
static void __attribute__((constructor, used)) system_web_handler_auto_init(void)
{
    web_module_register_handler("system", system_web_handler_init);
}
