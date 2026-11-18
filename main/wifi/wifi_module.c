/**
 * @file    wifi_module.c
 * @brief   WiFi 底层驱动实现
 *
 * 封装 esp_wifi 驱动的初始化、STA 连接、AP 配置、WiFi 扫描和事件分发。
 * 不含业务逻辑，仅作为驱动抽象层向上层（wifi_manager）提供能力。
 *
 * @date    2026-05-19
 * @author  Mr.Tang
 */

#include "wifi_module.h"

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/ip4_addr.h"

static const char *TAG = "[wifi_module]";

/* ========================================== 类型定义 ============================================= */

/* ========================================== 模块级静态变量 ============================================= */

static wifi_module_config_t s_wifi_cfg;     /**< 配置副本 */
static bool                 s_wifi_inited = false; /**< WiFi 模块初始化标志 */
static bool                 s_connecting  = false; /**< STA 正在尝试连接标志 */
static esp_netif_t         *s_sta_netif = NULL;   /**< STA 网络接口句柄 */
static esp_netif_t         *s_ap_netif  = NULL;   /**< AP 网络接口句柄 */

/* ========================================== 内部函数 ============================================= */

/**
 * @brief 统一转发 WiFi 模块事件到上层回调
 *
 * @param event 事件类型
 */
static void wifi_module_handle_event(wifi_module_event_t event)
{
    if (s_wifi_cfg.event_cb) {
        s_wifi_cfg.event_cb(event);
    }
}

/**
 * @brief WiFi 事件回调
 *
 * 仅处理 WIFI_EVENT，统一转换为 wifi_module_event_t 上报。
 *
 * @param arg        事件处理器注册参数（未使用）
 * @param event_base 事件基
 * @param event_id   事件 ID
 * @param event_data 事件数据（未使用）
 */
static void wifi_module_wifi_event_handler(void *arg,
                                           esp_event_base_t event_base,
                                           int32_t event_id,
                                           void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base != WIFI_EVENT) {
        return;
    }

    switch (event_id) {
    case WIFI_EVENT_WIFI_READY:
        break;

    case WIFI_EVENT_SCAN_DONE:
        break;

    case WIFI_EVENT_STA_START:
        break;

    case WIFI_EVENT_STA_STOP:
        break;

    case WIFI_EVENT_STA_CONNECTED:
        s_connecting = false;
        wifi_module_handle_event(WIFI_MODULE_EVENT_STA_CONNECTED);
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        if (s_connecting) {
            s_connecting = false;
            wifi_module_handle_event(WIFI_MODULE_EVENT_STA_CONNECT_FAILED);
        } else {
            wifi_module_handle_event(WIFI_MODULE_EVENT_STA_DISCONNECTED);
        }
        break;

    case WIFI_EVENT_STA_AUTHMODE_CHANGE:
        break;

    case WIFI_EVENT_AP_START:
        break;

    case WIFI_EVENT_AP_STOP:
        break;

    case WIFI_EVENT_AP_STACONNECTED:
        break;

    case WIFI_EVENT_AP_STADISCONNECTED:
        break;

    case WIFI_EVENT_AP_PROBEREQRECVED:
        break;

    default:
        break;
    }
}

/**
 * @brief IP 事件回调
 *
 * 仅对 STA 获取 IP 等关键事件做处理，其余保持占位便于后续扩展。
 *
 * @param arg        事件处理器注册参数（未使用）
 * @param event_base 事件基
 * @param event_id   事件 ID
 * @param event_data 事件数据（未使用）
 */
static void wifi_module_ip_event_handler(void *arg,
                                          esp_event_base_t event_base,
                                          int32_t event_id,
                                          void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base != IP_EVENT) {
        return;
    }

    switch (event_id) {
    case IP_EVENT_STA_GOT_IP:
        s_connecting = false;
        wifi_module_handle_event(WIFI_MODULE_EVENT_STA_GOT_IP);
        break;

    case IP_EVENT_STA_LOST_IP:
        break;

    case IP_EVENT_AP_STAIPASSIGNED:
        break;

    case IP_EVENT_GOT_IP6:
        break;

    case IP_EVENT_ETH_GOT_IP:
    case IP_EVENT_ETH_LOST_IP:
    case IP_EVENT_PPP_GOT_IP:
    case IP_EVENT_PPP_LOST_IP:
        break;

    default:
        break;
    }
}

/* ========================================== 公共 API ============================================= */

/**
 * @brief 参见 wifi_module.h
 */
esp_err_t wifi_module_init(const wifi_module_config_t *config)
{
    if (config == NULL) {
        s_wifi_cfg = WIFI_MODULE_DEFAULT_CONFIG();
    } else {
        s_wifi_cfg = *config;
    }

    if (s_wifi_inited) {
        return ESP_OK;
    }

    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop create failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (s_wifi_cfg.enable_sta && s_sta_netif == NULL) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
    }
    if (s_wifi_cfg.enable_ap && s_ap_netif == NULL) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
    }

    if (s_wifi_cfg.enable_ap && s_ap_netif != NULL && s_wifi_cfg.ap_ip[0] != '\0') {
        esp_netif_ip_info_t ip_info = {0}; // 清零初始化
        if (esp_netif_str_to_ip4(s_wifi_cfg.ap_ip, &ip_info.ip) == ESP_OK) {
            ip_info.gw = ip_info.ip;
            IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
            esp_netif_dhcps_stop(s_ap_netif);   // 1. 停止 DHCP 服务
            esp_netif_set_ip_info(s_ap_netif, &ip_info); // 2. 设置新 IP
            esp_netif_dhcps_start(s_ap_netif);  // 3. 重启 DHCP 服务
        }
    }

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&wifi_init_cfg);
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_INIT_STATE) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    wifi_mode_t mode = WIFI_MODE_NULL;
    if (s_wifi_cfg.enable_sta && s_wifi_cfg.enable_ap) {
        mode = WIFI_MODE_APSTA;
    } else if (s_wifi_cfg.enable_sta) {
        mode = WIFI_MODE_STA;
    } else if (s_wifi_cfg.enable_ap) {
        mode = WIFI_MODE_AP;
    }
    if (mode != WIFI_MODE_NULL) {
        ret = esp_wifi_set_mode(mode);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    if (s_wifi_cfg.enable_ap) {
        wifi_config_t ap_cfg = {0};

        strncpy((char *)ap_cfg.ap.ssid, s_wifi_cfg.ap_ssid, sizeof(ap_cfg.ap.ssid));
        ap_cfg.ap.ssid[sizeof(ap_cfg.ap.ssid) - 1] = '\0';
        ap_cfg.ap.ssid_len = strlen((char *)ap_cfg.ap.ssid);

        strncpy((char *)ap_cfg.ap.password, s_wifi_cfg.ap_password,
                sizeof(ap_cfg.ap.password));
        ap_cfg.ap.password[sizeof(ap_cfg.ap.password) - 1] = '\0';

        ap_cfg.ap.channel        = s_wifi_cfg.ap_channel;
        ap_cfg.ap.max_connection = s_wifi_cfg.max_sta_conn;

        ap_cfg.ap.authmode = (strlen(s_wifi_cfg.ap_password) == 0)
                             ? WIFI_AUTH_OPEN
                             : WIFI_AUTH_WPA_WPA2_PSK;

        ret = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_set_config(AP) failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &wifi_module_wifi_event_handler,
                                              NULL, NULL);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_handler_register(WIFI_EVENT) failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }
    ret = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                              &wifi_module_ip_event_handler,
                                              NULL, NULL);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_handler_register(IP_EVENT) failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_CONN) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_wifi_inited = true;
    return ESP_OK;
}

/**
 * @brief 参见 wifi_module.h
 */
esp_err_t wifi_module_connect(const char *ssid, const char *password)
{
    if (!s_wifi_inited) { // WiFi 模块未初始化
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_wifi_cfg.enable_sta) {
        return ESP_ERR_INVALID_STATE;
    }

    if (ssid == NULL || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t sta_cfg = {0};

    strncpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid));
    sta_cfg.sta.ssid[sizeof(sta_cfg.sta.ssid) - 1] = '\0';

    if (password != NULL) {
        strncpy((char *)sta_cfg.sta.password, password,
                sizeof(sta_cfg.sta.password));
        sta_cfg.sta.password[sizeof(sta_cfg.sta.password) - 1] = '\0';
    }

    // 设置全信道扫描 + 按信号强度排序，让 ESP32 自动选最强 AP
    sta_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    sta_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    esp_err_t   ret;
    wifi_mode_t mode = WIFI_MODE_NULL;

    wifi_ap_record_t ap_info;
    // 如果已有连接着的 AP，先断开
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        (void)esp_wifi_disconnect();
    }

    ret = esp_wifi_get_mode(&mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_get_mode failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (mode != WIFI_MODE_STA && mode != WIFI_MODE_APSTA) {
        // 根据初始化配置决定：含 AP 则升到 APSTA，否则升到 STA
        mode = s_wifi_cfg.enable_ap ? WIFI_MODE_APSTA : WIFI_MODE_STA;
        ret  = esp_wifi_set_mode(mode);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    ret = esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config(STA) failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_connecting = true;
    ret          = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(ret));
        s_connecting = false;
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief 参见 wifi_module.h
 */
esp_err_t wifi_module_scan(wifi_module_scan_result_t *results, uint16_t *count_inout)
{
    if (!s_wifi_inited || !s_wifi_cfg.enable_sta || results == NULL
        || count_inout == NULL || *count_inout == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_scan_config_t scan_cfg = {0};

    ESP_LOGI(TAG, "start wifi scan, max_out=%u", (unsigned)(*count_inout));

    esp_err_t ret = esp_wifi_scan_start(&scan_cfg, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi scan start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    uint16_t ap_num = 0;
    ret = esp_wifi_scan_get_ap_num(&ap_num);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi scan get num failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (ap_num == 0) {
        ESP_LOGI(TAG, "wifi scan done: found 0 AP");
        *count_inout = 0;
        return ESP_OK;
    }

    uint16_t max_out = *count_inout;
    if (max_out == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (ap_num > max_out) {
        ap_num = max_out;
    }

    wifi_ap_record_t *ap_list = heap_caps_calloc(ap_num, sizeof(wifi_ap_record_t),
                                                  MALLOC_CAP_SPIRAM);
    if (ap_list == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ret = esp_wifi_scan_get_ap_records(&ap_num, ap_list);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi scan get records failed: %s", esp_err_to_name(ret));
        free(ap_list);
        return ret;
    }

    for (uint16_t i = 0; i < ap_num; ++i) {
        memset(results[i].ssid, 0, sizeof(results[i].ssid));
        strncpy(results[i].ssid, (const char *)ap_list[i].ssid,
                sizeof(results[i].ssid) - 1);
        results[i].rssi     = ap_list[i].rssi;
        results[i].authmode = ap_list[i].authmode;
    }

    *count_inout = ap_num;
    ESP_LOGI(TAG, "wifi scan done: found %u AP(s), out=%u",
             (unsigned)ap_num, (unsigned)(*count_inout));
    free(ap_list);
    return ESP_OK;
}
