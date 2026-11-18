/**
 * @file    wifi_manager.c
 * @brief   WiFi 管理层（状态机）实现
 *
 * 协调 wifi_module、storage_module、web_module，
 * 实现断开重连、已保存 WiFi 优先级排序、用户手动连接保护等策略。
 *
 * 核心设计：
 * - 周期性状态机：每秒执行一次 wifi_manage_step()，根据当前状态决定行为
 * - 已保存 WiFi 遍历：按优先级（最近连接成功者排最高）顺序尝试
 * - 用户连接保护：用户通过 Web/API 手动发起连接时，状态机暂停自动遍历
 * - AP 延时关闭：连接成功后保持 AP 一段时间再切纯 STA 省电
 *
 * @date    2026-05-19
 * @author  Mr.Tang
 */

#include "wifi_manager.h"

#include "wifi_module.h"
#include "wifi_storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "event_bus.h"
#include "event_defs.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "mdns_service.h"

static const char *TAG = "[wifi_manage]";

/* ========================================== 配置宏 ============================================= */

/** @brief 连接成功后 AP 公告保持时间（ms），到期自动切换纯 STA */
#define AP_GRACE_PERIOD_MS 20000

/* ========================================== 类型定义 ============================================= */

/* ========================================== 模块级静态变量 ============================================= */

static wifi_manage_state_t  s_wifi_manage_state = WIFI_MANAGE_STATE_DISCONNECTED; /**< 当前管理状态 */
static wifi_manage_config_t s_wifi_cfg;                                           /**< 配置副本 */
static TaskHandle_t         s_wifi_manage_task  = NULL;                           /**< 管理任务句柄 */
static bool                 s_wifi_connecting   = false;                          /**< 是否有 STA 连接正在进行 */

/**
 * @brief 用户手动连接保护标志
 *
 * 为 true 时状态机暂停自动遍历已保存 WiFi，避免与用户手动连接冲突。
 * 用户在 Web/API 中发起连接时置位，连接成功/失败后清零。
 */
static bool                 s_user_initiated    = false;

static TickType_t           s_connected_ts      = 0;    /**< 最近一次成功连接的时间戳 */
static uint8_t              s_wifi_try_index    = 0;    /**< 本轮遍历中当前尝试的 WiFi 下标 */
static TickType_t           s_connect_failed_ts = 0;    /**< 最近一次全轮失败的时间戳 */
static bool                 s_skip_ap_once      = false; /**< 下一次 STA 断开时跳过 AP 开启 */

/* ========================================== 内部函数 ============================================= */

/**
 * @brief 统一更新状态并通知上层回调
 *
 * @param new_state 新状态
 */
static void wifi_manage_notify_state(wifi_manage_state_t new_state)
{
    s_wifi_manage_state = new_state;

    /* 通过事件总线通知其他模块 */
    switch (new_state) {
        case WIFI_MANAGE_STATE_CONNECTED:
            event_bus_post(EVENT_WIFI_CONNECTED, NULL, 0);
            break;
        case WIFI_MANAGE_STATE_DISCONNECTED:
            event_bus_post(EVENT_WIFI_DISCONNECTED, NULL, 0);
            break;
        case WIFI_MANAGE_STATE_CONNECT_FAILED:
            event_bus_post(EVENT_WIFI_CONNECT_FAILED, NULL, 0);
            break;
        default:
            break;
    }
}

/* -------------------- Web 回调：查询当前 WiFi 状态 -------------------- */

/**
 * @brief 提供给 Web 模块的 WiFi 状态查询回调
 *
 * 仅返回网页展示所需的少量字段：连接状态、SSID、IP、RSSI、模式、主机名。
 *
 * @param[out] out 输出状态结构体，不可为 NULL
 * @return
 *      - ESP_OK              查询成功
 *      - ESP_ERR_INVALID_ARG  out 为 NULL
 */
esp_err_t wifi_manage_get_web_status(web_wifi_status_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    out->connected = false;
    out->state     = WEB_WIFI_STATUS_STATE_IDLE;
    strncpy(out->ssid, "-", sizeof(out->ssid));
    out->ssid[sizeof(out->ssid) - 1] = '\0';
    strncpy(out->ip, "-", sizeof(out->ip));
    out->ip[sizeof(out->ip) - 1] = '\0';
    strncpy(out->mode, "-", sizeof(out->mode));
    out->mode[sizeof(out->mode) - 1] = '\0';
    strncpy(out->hostname, "-", sizeof(out->hostname));
    out->hostname[sizeof(out->hostname) - 1] = '\0';

    if (s_wifi_connecting) {
        out->state = WEB_WIFI_STATUS_STATE_CONNECTING;
    } else {
        switch (s_wifi_manage_state) {
        case WIFI_MANAGE_STATE_CONNECTED:
            out->state     = WEB_WIFI_STATUS_STATE_CONNECTED;
            out->connected = true;
            break;

        case WIFI_MANAGE_STATE_CONNECT_FAILED:
            out->state = WEB_WIFI_STATUS_STATE_FAILED;
            break;

        case WIFI_MANAGE_STATE_DISCONNECTED:
        default:
            out->state = WEB_WIFI_STATUS_STATE_IDLE;
            break;
        }
    }

    if (out->state != WEB_WIFI_STATUS_STATE_CONNECTED) {
        return ESP_OK;
    }

    wifi_ap_record_t ap_info = {0};
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        strncpy(out->ssid, (const char *)ap_info.ssid, sizeof(out->ssid));
        out->ssid[sizeof(out->ssid) - 1] = '\0';
        out->rssi = ap_info.rssi;
    }

    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif != NULL) {
        esp_netif_ip_info_t ip_info = {0};
        if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
            snprintf(out->ip, sizeof(out->ip), IPSTR, IP2STR(&ip_info.ip));
        }
    }

    wifi_mode_t wifi_mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&wifi_mode) == ESP_OK) {
        const char *mode_str = "-";

        switch (wifi_mode) {
        case WIFI_MODE_STA:
            mode_str = "STA";
            break;
        case WIFI_MODE_AP:
            mode_str = "AP";
            break;
        case WIFI_MODE_APSTA:
            mode_str = "AP+STA";
            break;
        default:
            break;
        }

        strncpy(out->mode, mode_str, sizeof(out->mode));
        out->mode[sizeof(out->mode) - 1] = '\0';
    }

    {
        const char *h = mdns_service_get_hostname();
        strncpy(out->hostname, h, sizeof(out->hostname) - 1);
        out->hostname[sizeof(out->hostname) - 1] = '\0';
    }

    return ESP_OK;
}

/* ========================================== 公共 API ============================================= */

/**
 * @brief 下一次 STA 断开时跳过 AP 开启
 *
 * 用于从 NCM 网卡模式退出时断开 WiFi 重连，不打开 AP。
 * 该标志在下一次 STA 断开事件处理中自动清除，仅生效一次。
 */
void wifi_manage_skip_ap_once(void)
{
    s_skip_ap_once = true;
}

/** @brief EVENT_NCM_EXIT 事件回调：断开 WiFi 并跳过 AP 开启 */
static void wifi_on_ncm_exit(const void *payload, size_t len, void *ctx)
{
    (void)payload; (void)len; (void)ctx;
    wifi_manage_skip_ap_once();
    esp_wifi_disconnect();
    ESP_LOGI(TAG, "NCM exited, WiFi disconnected (AP skip set)");
}

/**
 * @brief 获取 Web 配网端口
 * @return 端口号
 */
int wifi_manage_get_web_port(void)
{
    return s_wifi_cfg.web_port;
}

/* -------------------- Web 回调：已保存 WiFi 列表与删除 -------------------- */

/**
 * @brief 提供给 Web 模块的已保存 WiFi 列表回调
 *
 * @param[in,out] list      输出缓冲区，NULL 表示仅查询数量
 * @param[in,out] inout_cnt 入口为容量，出口为实际数量
 * @return
 *      - ESP_OK              成功
 *      - ESP_ERR_INVALID_ARG  inout_cnt 为 NULL
 *      - ESP_ERR_NO_MEM       内存不足
 */
esp_err_t wifi_manage_get_web_saved_list(web_saved_wifi_info_t *list,
                                         size_t *inout_cnt)
{
    if (inout_cnt == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t max_num = (s_wifi_cfg.save_wifi_count <= 0) ? 1
                      : (uint8_t)s_wifi_cfg.save_wifi_count;

    if (max_num == 0) {
        *inout_cnt = 0;
        return ESP_OK;
    }

    wifi_config_t *configs = (wifi_config_t *)heap_caps_malloc(
        max_num * sizeof(wifi_config_t), MALLOC_CAP_SPIRAM);
    if (configs == NULL) {
        *inout_cnt = 0;
        return ESP_ERR_NO_MEM;
    }

    uint8_t   count = 0;
    esp_err_t ret   = wifi_storage_load_all(configs, &count);
    if (ret != ESP_OK) {
        free(configs);
        *inout_cnt = 0;
        return ret;
    }

    if (count > max_num) {
        count = max_num;
    }

    if (list == NULL) {
        *inout_cnt = count;
        free(configs);
        return ESP_OK;
    }

    size_t cap = *inout_cnt;
    if (cap == 0) {
        free(configs);
        *inout_cnt = 0;
        return ESP_ERR_INVALID_ARG;
    }
    if (cap > count) {
        cap = count;
    }

    for (size_t i = 0; i < cap; i++) {
        strncpy(list[i].ssid, (const char *)configs[i].sta.ssid,
                sizeof(list[i].ssid));
        list[i].ssid[sizeof(list[i].ssid) - 1] = '\0';
    }

    free(configs);
    *inout_cnt = cap;
    return ESP_OK;
}

/**
 * @brief 提供给 Web 模块的删除已保存 WiFi 回调
 *
 * @param ssid 目标 SSID
 * @return ESP_OK 成功，其他为存储层错误
 */
esp_err_t wifi_manage_delete_web_saved(const char *ssid)
{
    return wifi_storage_delete_by_ssid(ssid);
}

/* -------------------- Web 回调：扫描附近 WiFi -------------------- */

/**
 * @brief 提供给 Web 模块的附近 WiFi 扫描回调
 *
 * 通过 wifi_module_scan 获取 AP 列表并转换为 Web 展示所需字段。
 *
 * @param[in,out] list      输出缓冲区
 * @param[in,out] inout_cnt 入口为容量，出口为实际数量
 * @return
 *      - ESP_OK              成功
 *      - ESP_ERR_INVALID_ARG  参数非法
 *      - ESP_ERR_NO_MEM       内存不足
 */
esp_err_t wifi_manage_scan_web(web_scan_result_t *list, size_t *inout_cnt)
{
    if (list == NULL || inout_cnt == NULL || *inout_cnt == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "web scan request: max_cnt=%u", (unsigned)(*inout_cnt));

    uint16_t cap = (uint16_t)(*inout_cnt);
    wifi_module_scan_result_t *results = (wifi_module_scan_result_t *)heap_caps_malloc(
        cap * sizeof(wifi_module_scan_result_t), MALLOC_CAP_SPIRAM);
    if (results == NULL) {
        *inout_cnt = 0;
        return ESP_ERR_NO_MEM;
    }

    uint16_t   count = cap;
    esp_err_t  ret   = wifi_module_scan(results, &count);
    if (ret != ESP_OK) {
        free(results);
        *inout_cnt = 0;
        return ret;
    }

    ESP_LOGI(TAG, "wifi scan done: count=%u", (unsigned)count);

    size_t out_cnt = (size_t)count;
    if (out_cnt > *inout_cnt) {
        out_cnt = *inout_cnt;
    }

    for (size_t i = 0; i < out_cnt; i++) {
        strncpy(list[i].ssid, results[i].ssid, sizeof(list[i].ssid));
        list[i].ssid[sizeof(list[i].ssid) - 1] = '\0';
        list[i].rssi = results[i].rssi;
        list[i].authmode = results[i].authmode;
    }

    free(results);
    *inout_cnt = out_cnt;
    return ESP_OK;
}

/* -------------------- Web 回调：从已保存列表连接 -------------------- */

/**
 * @brief 提供给 Web 模块的连接已保存 WiFi 回调
 *
 * 实现策略：
 * 1. 在存储列表中定位目标 SSID，通过 wifi_storage_on_connected() 提升优先级
 * 2. 主动断开当前 STA，由状态机在收到断开事件后按新优先级自动重连
 *
 * 不直接操控状态机，通过「提升优先级 + 触发断开」驱动后续行为。
 *
 * @param ssid 目标 SSID
 * @return
 *      - ESP_OK              成功提交
 *      - ESP_ERR_INVALID_ARG  ssid 非法
 *      - ESP_ERR_NO_MEM       内存不足
 *      - ESP_ERR_NOT_FOUND    SSID 不在已保存列表中
 */
esp_err_t wifi_manage_connect_web_saved(const char *ssid)
{
    if (ssid == NULL || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t max_num = (s_wifi_cfg.save_wifi_count <= 0) ? 1
                      : (uint8_t)s_wifi_cfg.save_wifi_count;

    wifi_config_t *configs = (wifi_config_t *)heap_caps_malloc(
        max_num * sizeof(wifi_config_t), MALLOC_CAP_SPIRAM);
    if (configs == NULL) {
        return ESP_ERR_NO_MEM;
    }

    uint8_t   count = 0;
    esp_err_t ret   = wifi_storage_load_all(configs, &count);
    if (ret != ESP_OK || count == 0) {
        free(configs);
        return ret;
    }

    int found_index = -1;
    for (uint8_t i = 0; i < count; i++) {
        if (configs[i].sta.ssid[0] == '\0') {
            continue;
        }
        if (strncmp((const char *)configs[i].sta.ssid, ssid,
                    sizeof(configs[i].sta.ssid)) == 0) {
            found_index = (int)i;
            break;
        }
    }

    if (found_index < 0) {
        free(configs);
        return ESP_ERR_NOT_FOUND;
    }

    ret = wifi_storage_on_connected(&configs[found_index]);
    free(configs);

    if (ret != ESP_OK) {
        return ret;
    }

    (void)esp_wifi_disconnect();

    return ESP_OK;
}

/* -------------------- Web 回调：表单连接 WiFi -------------------- */

/**
 * @brief 提供给 Web 模块的表单连接 WiFi 回调
 *
 * 标记为用户手动发起（s_user_initiated = true），保护连接流程不被状态机自动遍历干扰。
 *
 * @param ssid     目标 SSID
 * @param password 目标密码，NULL 表示开放网络
 * @return
 *      - ESP_OK              成功提交
 *      - ESP_ERR_INVALID_ARG  ssid 非法
 *      - 其他                 底层 wifi_module_connect 错误
 */
esp_err_t wifi_manage_connect_web_form(const char *ssid, const char *password)
{
    if (ssid == NULL || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    const char *pwd = (password != NULL && password[0] != '\0') ? password : NULL;

    s_user_initiated = true; // 标记为用户手动连接，暂停状态机自动遍历
    return wifi_module_connect(ssid, pwd);
}

/* -------------------- WiFi 模块事件回调 -------------------- */

/**
 * @brief WiFi 模块事件回调，驱动管理状态机
 *
 * @param event WiFi 模块上报的事件
 */
static void wifi_manage_on_wifi_event(wifi_module_event_t event)
{
    switch (event) {
    case WIFI_MODULE_EVENT_STA_CONNECTED:
        break;

    case WIFI_MODULE_EVENT_STA_GOT_IP: {
        wifi_manage_notify_state(WIFI_MANAGE_STATE_CONNECTED);
        s_wifi_connecting   = false;
        s_wifi_try_index    = 0;
        s_connect_failed_ts = 0;
        s_connected_ts      = xTaskGetTickCount();
        s_user_initiated    = false; // 连接成功，清除用户手动连接标志

        wifi_config_t current_cfg = {0};
        if (esp_wifi_get_config(WIFI_IF_STA, &current_cfg) == ESP_OK) {
            (void)wifi_storage_on_connected(&current_cfg);
        }

        break;
    }

    case WIFI_MODULE_EVENT_STA_DISCONNECTED:
        wifi_manage_notify_state(WIFI_MANAGE_STATE_DISCONNECTED);
        s_wifi_connecting = false;
        s_wifi_try_index  = 0;
        
        if (!s_skip_ap_once) {  
            esp_wifi_set_mode(WIFI_MODE_APSTA); // 断开后切回 APSTA，保留 AP 配网通道
        }
        break;

    case WIFI_MODULE_EVENT_STA_CONNECT_FAILED:
        s_wifi_connecting = false;

        if (s_user_initiated) {
            // 用户手动连接失败，立即通知上层，不等状态机周期
            wifi_manage_notify_state(WIFI_MANAGE_STATE_CONNECT_FAILED);
            s_connect_failed_ts = xTaskGetTickCount();
        }
        s_user_initiated = false; // 清除用户手动连接标志
        s_wifi_try_index++;

        esp_wifi_set_mode(WIFI_MODE_APSTA); // 连接失败后切回 APSTA，保留 AP 配网通道
        break;

    default:
        break;
    }
}

/* -------------------- 状态机核心逻辑 -------------------- */

/**
 * @brief 单步执行 WiFi 管理状态机
 *
 * 按当前状态决定操作：发起连接、等待结果、切换状态、等待重试。
 */
static void wifi_manage_step(void)
{
    switch (s_wifi_manage_state) {
    case WIFI_MANAGE_STATE_DISCONNECTED: {
        if (s_wifi_connecting) {
            break; // 已有连接操作进行中，等待事件回调
        }

        if (s_user_initiated) {
            break; // 用户手动连接保护：暂停自动遍历
        }

        uint8_t max_num = (s_wifi_cfg.save_wifi_count <= 0) ? 1
                          : (uint8_t)s_wifi_cfg.save_wifi_count;

        wifi_config_t *list = (wifi_config_t *)heap_caps_malloc(
            max_num * sizeof(wifi_config_t), MALLOC_CAP_SPIRAM);
        if (list == NULL) {
            break;
        }

        uint8_t count = 0;

        if (wifi_storage_load_all(list, &count) != ESP_OK || count == 0) {
            free(list);
            break;
        }

        if (s_wifi_try_index >= count) {
            wifi_manage_notify_state(WIFI_MANAGE_STATE_CONNECT_FAILED);
            s_connect_failed_ts = xTaskGetTickCount();
            s_wifi_try_index    = 0;
            s_wifi_connecting   = false;
            free(list);
            break;
        }

        wifi_config_t *cfg = &list[s_wifi_try_index];
        if (cfg->sta.ssid[0] == '\0') {
            s_wifi_try_index++;
            free(list);
            break;
        }

        const char *ssid     = (const char *)cfg->sta.ssid;
        const char *password = (cfg->sta.password[0] == '\0')
                               ? NULL : (const char *)cfg->sta.password;

        if (wifi_module_connect(ssid, password) == ESP_OK) {
            s_wifi_connecting = true;
        } else {
            s_wifi_try_index++;
        }

        free(list);
        break;
    }

    case WIFI_MANAGE_STATE_CONNECTED: {
        wifi_mode_t mode = WIFI_MODE_NULL;
        if (esp_wifi_get_mode(&mode) == ESP_OK && mode == WIFI_MODE_APSTA) {
            TickType_t now     = xTaskGetTickCount();
            TickType_t elapsed = now - s_connected_ts;
            TickType_t grace   = pdMS_TO_TICKS(AP_GRACE_PERIOD_MS);

            if (elapsed >= grace) {
                ESP_LOGI(TAG, "AP grace period expired, switching to STA-only mode");
                esp_wifi_set_mode(WIFI_MODE_STA);
            }
        }
        break;
    }

    case WIFI_MANAGE_STATE_CONNECT_FAILED: {
        if (s_wifi_cfg.reconnect_interval_ms < 0) {
            break; // 关闭自动重连
        }

        TickType_t now   = xTaskGetTickCount();
        TickType_t delta = now - s_connect_failed_ts;
        TickType_t need  = pdMS_TO_TICKS(
            (s_wifi_cfg.reconnect_interval_ms <= 0)
            ? 0 : s_wifi_cfg.reconnect_interval_ms);

        if (delta >= need) {
            s_wifi_try_index    = 0;
            s_wifi_connecting   = false;
            wifi_manage_notify_state(WIFI_MANAGE_STATE_DISCONNECTED);
        }
        break;
    }

    default:
        break;
    }
}

/* -------------------- WiFi 管理任务 -------------------- */

/**
 * @brief 管理任务：周期性驱动状态机
 *
 * @param arg 未使用
 */
static void wifi_manage_task(void *arg)
{
    (void)arg;

    for (;;) {
        wifi_manage_step();
        vTaskDelay(pdMS_TO_TICKS(WIFI_MANAGE_STEP_INTERVAL_MS));
    }
}

/* ========================================== 公共 API ============================================= */

/**
 * @brief 参见 wifi_manager.h
 */
esp_err_t wifi_manage_init(const wifi_manage_config_t *config)
{
    if (config == NULL) {
        s_wifi_cfg = WIFI_MANAGE_DEFAULT_CONFIG();
    } else {
        s_wifi_cfg = *config;
    }

    /* ---- 初始化 WiFi 模块 ---- */
    wifi_module_config_t wifi_cfg = WIFI_MODULE_DEFAULT_CONFIG();
    wifi_cfg.enable_sta = true;
    wifi_cfg.enable_ap  = true;

    strncpy(wifi_cfg.ap_ssid, s_wifi_cfg.ap_ssid, sizeof(wifi_cfg.ap_ssid));
    wifi_cfg.ap_ssid[sizeof(wifi_cfg.ap_ssid) - 1] = '\0';

    strncpy(wifi_cfg.ap_password, s_wifi_cfg.ap_password,
            sizeof(wifi_cfg.ap_password));
    wifi_cfg.ap_password[sizeof(wifi_cfg.ap_password) - 1] = '\0';

    strncpy(wifi_cfg.ap_ip, s_wifi_cfg.ap_ip, sizeof(wifi_cfg.ap_ip));
    wifi_cfg.ap_ip[sizeof(wifi_cfg.ap_ip) - 1] = '\0';

    wifi_cfg.event_cb = wifi_manage_on_wifi_event;

    esp_err_t ret = wifi_module_init(&wifi_cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    /* ---- 初始化存储模块 ---- */
    wifi_storage_config_t storage_cfg = WIFI_STORAGE_DEFAULT_CONFIG();

    storage_cfg.max_wifi_num = (s_wifi_cfg.save_wifi_count <= 0)
                               ? 1 : (uint8_t)s_wifi_cfg.save_wifi_count;

    ret = wifi_storage_init(&storage_cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    /* ---- 根据 NVS 中已有 WiFi 决定初始模式 ---- */
    {
        uint8_t max_num = (s_wifi_cfg.save_wifi_count <= 0) ? 1
                          : (uint8_t)s_wifi_cfg.save_wifi_count;
        wifi_config_t *dummy = (wifi_config_t *)heap_caps_malloc(
            max_num * sizeof(wifi_config_t), MALLOC_CAP_SPIRAM);
        if (dummy != NULL) {
            uint8_t saved_count = 0;
            if (wifi_storage_load_all(dummy, &saved_count) == ESP_OK
                && saved_count > 0) {
                ESP_LOGI(TAG, "Found %u saved WiFi config(s), switching to STA-only mode",
                         saved_count);
                esp_wifi_set_mode(WIFI_MODE_STA);
            } else {
                ESP_LOGI(TAG, "No saved WiFi found, keeping APSTA mode for provisioning");
                wifi_manage_notify_state(WIFI_MANAGE_STATE_DISCONNECTED);
            }
            free(dummy);
        }
    }

    /* ---- 注册 NCM 退出事件监听 ---- */
    event_bus_register(EVENT_NCM_EXIT, wifi_on_ncm_exit, NULL);

    if (s_wifi_manage_task == NULL) {
        BaseType_t ret_task = xTaskCreate(
            wifi_manage_task,
            "wifi_manage",
            4096,
            NULL,
            tskIDLE_PRIORITY + 1,
            &s_wifi_manage_task);

        if (ret_task != pdPASS) {
            return ESP_ERR_NO_MEM;
        }
    }

    return ESP_OK;
}