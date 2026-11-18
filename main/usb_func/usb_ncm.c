/**
 * @file    usb_ncm.c
 * @brief   USB NCM 网卡桥接模块实现
 *
 * 基于 ESP-IDF TinyUSB NCM 示例（tusb_ncm）移植，
 * 适配 SuperDongle 现有 WiFi 管理架构。
 *
 * 核心逻辑：
 *   - WiFi STA RX → esp_wifi_internal_reg_rxcb → pkt_wifi2usb() → tinyusb_net_send_sync()
 *   - USB NCM RX  → usb_recv_callback() → esp_wifi_internal_tx()
 *
 * 注意：进入 NCM 模式后，WiFi STA 的所有 lwIP 流量被拦截，
 * 设备自身的 Web 服务、mDNS 等均停止工作。
 *
 * @date    2026-06-08
 * @author  Mr.Tang
 */

#include "usb_ncm.h"

#if CONFIG_TINYUSB_NET_MODE_NCM

#include <string.h>

#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_pm.h"
#include "esp_wifi.h"
#include "esp_private/wifi.h"

#include "tusb.h"
#include "tinyusb.h"
#include "tinyusb_net.h"

static const char *TAG = "[usb_ncm]";

/* ========================================== 模块级静态变量 ============================================= */

static bool s_ncm_initialized = false;
static bool s_wifi_connected  = false;
static esp_pm_lock_handle_t s_pm_lock = NULL;

/* ========================================== 数据包桥接回调 ============================================= */

/**
 * @brief USB NCM 收到数据 → 转发到 WiFi STA TX
 *
 * 由 TinyUSB 协议栈在 USB 主机发送数据时回调。
 *
 * @param buffer 数据缓冲区
 * @param len    数据长度
 * @param ctx    用户上下文（指向 s_wifi_connected）
 * @return ESP_OK
 */
static esp_err_t usb_recv_callback(void *buffer, uint16_t len, void *ctx)
{
    bool *is_wifi_connected = (bool *)ctx;
    if (*is_wifi_connected) {
        esp_wifi_internal_tx(ESP_IF_WIFI_STA, buffer, len);
    }
    return ESP_OK;
}

/**
 * @brief 释放 WiFi TX 缓冲区
 *
 * 当 tinyusb_net_send_sync 成功消费数据包后调用。
 *
 * @param eb  待释放的 WiFi buffer
 * @param ctx 用户上下文（未使用）
 */
static void wifi_pkt_free(void *eb, void *ctx)
{
    (void)ctx;
    esp_wifi_internal_free_rx_buffer(eb);
}

/**
 * @brief WiFi STA 收到数据 → 转发到 USB NCM
 *
 * 通过 esp_wifi_internal_reg_rxcb 注册，在 WiFi STA 收到数据包时回调。
 *
 * @param buffer 数据缓冲区
 * @param len    数据长度
 * @param eb     WiFi buffer 句柄（用于释放）
 * @return ESP_OK
 */
static esp_err_t pkt_wifi2usb(void *buffer, uint16_t len, void *eb)
{
    if (tinyusb_net_send_sync(buffer, len, eb, portMAX_DELAY) != ESP_OK) {
        esp_wifi_internal_free_rx_buffer(eb);
    }
    return ESP_OK;
}

/* ========================================== WiFi 事件处理 ============================================= */

/**
 * @brief WiFi 事件回调
 *
 * 监听 STA 连接/断开事件，同步 USB NCM 链路状态。
 *
 * @param arg       用户参数（未使用）
 * @param event_base 事件基
 * @param event_id  事件 ID
 * @param event_data 事件数据
 */
static void ncm_wifi_event_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi STA disconnected, NCM link down");
        s_wifi_connected = false;
        esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, NULL);
        tud_network_link_state(0, false);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "WiFi STA connected, NCM link up");
        esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, pkt_wifi2usb);
        s_wifi_connected = true;
        tud_network_link_state(0, true);
    }
}

/* ========================================== 公共 API ============================================= */

esp_err_t usb_ncm_init(void)
{
    if (s_ncm_initialized) {
        ESP_LOGW(TAG, "NCM already initialized");
        return ESP_OK;
    }

    if (s_pm_lock == NULL) {
        esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "ncm", &s_pm_lock);
    }

    /* 注册 WiFi 事件监听（多个 handler 可共存，不影响 wifi_manager） */
    esp_err_t ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                ncm_wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 初始化 TinyUSB NCM 设备类 */
    tinyusb_net_config_t net_config = {
        .on_recv_callback = usb_recv_callback,
        .free_tx_buffer   = wifi_pkt_free,
        .user_context     = &s_wifi_connected,
    };
    esp_read_mac(net_config.mac_addr, ESP_MAC_WIFI_STA);
    uint8_t *mac = net_config.mac_addr;
    ESP_LOGI(TAG, "NCM MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ret = tinyusb_net_init(&net_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_net_init failed: %s", esp_err_to_name(ret));
        esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, ncm_wifi_event_handler);
        return ret;
    }

    /* 根据当前 WiFi 连接状态设置 NCM 链路状态 */
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        /* WiFi 已连接，立即注册 RX 回调并启用 NCM 链路 */
        esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, pkt_wifi2usb);
        s_wifi_connected = true;
        tud_network_link_state(0, true);
        ESP_LOGI(TAG, "WiFi already connected, NCM link up");
    } else {
        /* WiFi 未连接，等待 wifi_manager 自动重连 */
        s_wifi_connected = false;
        ESP_LOGI(TAG, "WiFi not connected yet, waiting for connection...");
    }

    s_ncm_initialized = true;
    esp_pm_lock_acquire(s_pm_lock);
    ESP_LOGI(TAG, "NCM bridge initialized");
    return ESP_OK;
}

void usb_ncm_deinit(void)
{
    if (!s_ncm_initialized) {
        return;
    }

    /* 先注销 WiFi 事件监听，防止事件 handler 在清理窗口期内重新注册 RX 回调 */
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, ncm_wifi_event_handler);

    /* 取消 WiFi STA RX 回调，恢复 lwIP 正常收包 */
    esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, NULL);

    /* 禁用 NCM 链路 */
    tud_network_link_state(0, false);

    /* 反初始化 TinyUSB NCM */
    tinyusb_net_deinit();

    esp_pm_lock_release(s_pm_lock);

    s_wifi_connected  = false;
    s_ncm_initialized = false;
    ESP_LOGI(TAG, "NCM bridge deinitialized");
}

#endif /* CONFIG_TINYUSB_NET_MODE_NCM */
