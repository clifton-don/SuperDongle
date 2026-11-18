/**
 * @file    http_ota_manager.c
 * @brief   HTTP OTA 升级管理器实现
 *
 * 通过 HTTP 从远程 OTA 服务器获取固件和 SPIFFS 文件系统的更新。
 * 主要设计：
 *   1. 从 version.json 获取远程版本信息
 *   2. 对比本地固件版本号判断是否有更新
 *   3. 固件 OTA 通过 esp_ota_* API，SPIFFS OTA 通过直接分区读写
 *   4. SPIFFS A/B 双分区设计，通过 NVS 记录活跃分区索引（不记录版本号）
 *
 * SPIFFS A/B 的 NVS 操作函数在本文件重复实现的原因：
 *   本模块是独立 ESP-IDF 组件（components/ota_manager/），
 *   不能依赖主项目（main/）中的 storage_module，否则造成循环引用。
 *   两边各自实现相同的 NVS 读写逻辑，使用同一命名空间 "spiffs_ota"，独立工作互不冲突。
 *
 * 版本号管理：
 *   - 设备只有一个版本号：固件版本（CONFIG_APP_PROJECT_VER）
 *   - SPIFFS 不独立追踪版本号，由 version.json 的 version 统一管理
 *
 * @date    2026-05-19
 * @author  Mr.Tang
 */

#include "http_ota_manager.h"

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "cJSON.h"

#include "event_bus.h"
#include "event_defs.h"

static const char *TAG = "[ota_manager]";

/* ========================================== 配置宏 ============================================= */

#ifndef CONFIG_APP_PROJECT_VER
#define CONFIG_APP_PROJECT_VER "0.0.0"
#endif

/** @brief HTTPS 证书绑定 */
extern esp_err_t esp_crt_bundle_attach(void *conf);

/** @brief NVS namespace for OTA version URL config */
#define OTA_CONFIG_NVS "ota_config"
#define OTA_CONFIG_KEY "version_url"

/* ========================================== SPIFFS A/B NVS 管理 ============================================= */

/**
 * SPIFFS A/B 双分区通过 NVS 记录当前活跃分区索引：
 *   - 索引 0 → spiffs_0，索引 1 → spiffs_1
 *   - 默认活跃分区为 spiffs_1（PlatformIO uploadfs 默认写入目标）
 *   - OTA 更新时写入非活跃分区，成功后将活跃标志切换到新分区
 */

#define SPIFFS_OTA_NVS "spiffs_ota"

/** @brief 获取当前活跃的 SPIFFS 分区索引 */
static int spiffs_ota_get_active_idx(void)
{
    nvs_handle_t h;
    uint8_t      active = 0;
    esp_err_t    err    = nvs_open(SPIFFS_OTA_NVS, NVS_READONLY, &h);
    if (err == ESP_OK) {
        err = nvs_get_u8(h, "active", &active);
        nvs_close(h);
    }
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return 1;
    }
    return (active == 1) ? 1 : 0;
}

/** @brief 设置活跃的 SPIFFS 分区索引并写入 NVS */
static esp_err_t spiffs_ota_set_active(int idx)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(SPIFFS_OTA_NVS, NVS_READWRITE, &h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = nvs_set_u8(h, "active", (uint8_t)(idx == 1 ? 1 : 0));
    if (ret == ESP_OK) {
        ret = nvs_commit(h);
    }
    nvs_close(h);
    return ret;
}

/* ========================================== OTA 管理器全局状态 ============================================= */

/** @brief OTA 管理器全局状态结构体 */
static struct {
    char version_url[OTA_VERSION_URL_MAX];
    void (*state_cb)(int state, const char *msg);

    char running_version[32];           /**< 当前固件版本 */
    char new_version[32];               /**< 远程固件版本 */
    char new_url[OTA_VERSION_URL_MAX];  /**< 固件下载地址 */
    char new_spiffs_url[OTA_VERSION_URL_MAX]; /**< SPIFFS 下载地址 */
    char new_desc[OTA_DESC_MAX];        /**< 更新说明 */

    bool firmware_needs_update;         /**< 固件是否需要更新 */
    bool spiffs_needs_update;           /**< SPIFFS 是否需要更新 */

    bool update_available;
    bool busy;
    int  progress;
    bool cancel_requested;
    bool error;
    char status_msg[64];
} s_ota;

/* ========================================== 内部函数 ============================================= */

/** @brief 前向声明：执行 OTA 更新（实现在文件底部，被 ota_update_task 调用） */
static esp_err_t http_ota_manager_do_update(void);

/**
 * @brief 通知状态变化
 *
 * @param state 状态码（0=无更新，1=有更新，2=下载中，3=完成，-1=错误）
 * @param msg   状态描述文本
 */
static void notify(int state, const char *msg)
{
    if (state < 0) {
        s_ota.error = true;
    }
    if (s_ota.state_cb) {
        s_ota.state_cb(state, msg);
    }
    ESP_LOGI(TAG, "state=%d msg=%s", state, msg ? msg : "");
}

/* ========================================== 自动检查更新 ============================================= */

static TaskHandle_t s_ota_task = NULL;
static bool         s_event_registered = false;

/**
 * @brief 解析 version_url：优先 NVS 自定义值，回退到编译期默认值
 *
 * @param buf  输出缓冲区
 * @param size 缓冲区大小
 */
static void resolve_version_url(char *buf, size_t size)
{
    nvs_handle_t h;
    esp_err_t    err = nvs_open(OTA_CONFIG_NVS, NVS_READONLY, &h);
    if (err == ESP_OK) {
        size_t len = size;
        err = nvs_get_str(h, OTA_CONFIG_KEY, buf, &len);
        nvs_close(h);
        if (err == ESP_OK && buf[0] != '\0') {
            ESP_LOGI(TAG, "Using custom version_url from NVS: %s", buf);
            return;
        }
    }
    /* Fallback to compile-time default */
    strncpy(buf, HTTP_OTA_MANAGER_VERSION_URL, size - 1);
    buf[size - 1] = '\0';
}

/** @brief 自动检查更新任务 */
static void ota_auto_check_task(void *pvParameters)
{
    http_ota_manager_config_t ota_cfg = HTTP_OTA_MANAGER_DEFAULT_CONFIG();
    resolve_version_url(ota_cfg.version_url, sizeof(ota_cfg.version_url));
    http_ota_manager_init(&ota_cfg);
    http_ota_manager_check();
    s_ota_task = NULL;
    vTaskDelete(NULL);
}

/** @brief EVENT_WIFI_CONNECTED 事件回调：启动 OTA 自动检查 */
static void ota_on_wifi_connected(const void *payload, size_t len, void *ctx)
{
    (void)payload; (void)len; (void)ctx;
    if (s_ota_task == NULL) {
        xTaskCreate(ota_auto_check_task, "ota_check", 8192, NULL, 6, &s_ota_task);
    }
}

/** @brief 参见 http_ota_manager.h */
esp_err_t http_ota_manager_auto_init(void)
{
    if (!s_event_registered) {
        s_event_registered = true;
        event_bus_register(EVENT_WIFI_CONNECTED, ota_on_wifi_connected, NULL);
        ESP_LOGI(TAG, "registered EVENT_WIFI_CONNECTED listener");
    }
    return ESP_OK;
}

/* ========================================== NVS 版本 URL 管理 ============================================= */

esp_err_t http_ota_manager_get_version_url(char *buf, size_t size)
{
    if (!buf || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 先尝试 NVS */
    nvs_handle_t h;
    esp_err_t    err = nvs_open(OTA_CONFIG_NVS, NVS_READONLY, &h);
    if (err == ESP_OK) {
        size_t len = size;
        err = nvs_get_str(h, OTA_CONFIG_KEY, buf, &len);
        nvs_close(h);
        if (err == ESP_OK && buf[0] != '\0') {
            return ESP_OK;
        }
    }

    /* 回退到编译期默认值 */
    strncpy(buf, HTTP_OTA_MANAGER_VERSION_URL, size - 1);
    buf[size - 1] = '\0';
    return ESP_OK;
}

esp_err_t http_ota_manager_set_version_url(const char *url)
{
    if (!url) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t h;
    esp_err_t    err = nvs_open(OTA_CONFIG_NVS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    /* 空字符串 → 清除自定义设置，恢复默认 */
    err = nvs_set_str(h, OTA_CONFIG_KEY, url);
    if (err == ESP_OK) {
        err = nvs_commit(h);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "version_url saved to NVS: %s",
                     url[0] ? url : "(cleared, using default)");
        }
    }

    nvs_close(h);
    return err;
}

/* ========================================== 启动更新 / 取消更新 ============================================= */

static TaskHandle_t s_ota_update_task = NULL;

/** @brief OTA 更新执行任务 */
static void ota_update_task(void *pvParameters)
{
    http_ota_manager_do_update();
    s_ota.update_available      = false;
    s_ota.firmware_needs_update = false;
    s_ota.spiffs_needs_update   = false;
    s_ota_update_task           = NULL;
    vTaskDelete(NULL);
}

/** @brief 参见 http_ota_manager.h */
esp_err_t http_ota_manager_start_update(void)
{
    if (s_ota_update_task != NULL) {
        ESP_LOGW(TAG, "ota update already in progress");
        return ESP_FAIL;
    }
    if (!s_ota.update_available) {
        ESP_LOGW(TAG, "no update available");
        return ESP_FAIL;
    }
    s_ota.busy            = true;
    s_ota.progress        = 0;
    s_ota.cancel_requested = false;
    s_ota.error           = false;
    s_ota.status_msg[0]   = '\0';

    strncpy(s_ota.status_msg,
            s_ota.firmware_needs_update ? "firmware" : "spiffs",
            sizeof(s_ota.status_msg) - 1);
    notify(2, "Downloading...");
    xTaskCreate(ota_update_task, "ota_update", 8192, NULL, 5, &s_ota_update_task);
    return ESP_OK;
}

/** @brief 参见 http_ota_manager.h */
esp_err_t http_ota_manager_cancel(void)
{
    if (!s_ota.busy) {
        return ESP_OK;
    }
    s_ota.cancel_requested = true;
    ESP_LOGI(TAG, "cancel requested");
    return ESP_OK;
}

/* ========================================== 语义化版本号比较 ============================================= */

/**
 * @brief 比较两个语义化版本号
 *
 * @param v1 版本号字符串，NULL 视为 "0"
 * @param v2 版本号字符串，NULL 视为 "0"
 * @return
 *      - 1  v1 > v2
 *      - 0  v1 == v2
 *      - -1 v1 < v2
 */
static int version_compare(const char *v1, const char *v2)
{
    if (!v1) v1 = "0";
    if (!v2) v2 = "0";

    while (*v1 && *v2) {
        long n1 = strtol(v1, (char **)&v1, 10);
        long n2 = strtol(v2, (char **)&v2, 10);
        if (n1 != n2) return (n1 > n2) ? 1 : -1;
        if (*v1 == '.') v1++;
        if (*v2 == '.') v2++;
    }
    return (*v1) ? 1 : ((*v2) ? -1 : 0);
}

/* ========================================== 初始化 ============================================= */

/** @brief 参见 http_ota_manager.h */
esp_err_t http_ota_manager_init(const http_ota_manager_config_t *cfg)
{
    memset(&s_ota, 0, sizeof(s_ota));

    if (cfg) {
        strncpy(s_ota.version_url, cfg->version_url, sizeof(s_ota.version_url) - 1);
        s_ota.state_cb = cfg->state_cb;
    }

    strncpy(s_ota.running_version, CONFIG_APP_PROJECT_VER,
            sizeof(s_ota.running_version) - 1);

    ESP_LOGI(TAG, "init ok, version=%s", s_ota.running_version);
    return ESP_OK;
}

/* ========================================== 状态查询 API ============================================= */

const char *http_ota_manager_get_current_version(void)
{
    return s_ota.running_version;
}

const char *http_ota_manager_get_new_version(void)
{
    return s_ota.update_available ? s_ota.new_version : NULL;
}

const char *http_ota_manager_get_update_desc(void)
{
    return s_ota.update_available ? s_ota.new_desc : NULL;
}

const char *http_ota_manager_get_update_url(void)
{
    return s_ota.update_available ? s_ota.new_url : NULL;
}

const char *http_ota_manager_get_spiffs_url(void)
{
    return s_ota.new_spiffs_url[0] ? s_ota.new_spiffs_url : NULL;
}

bool http_ota_manager_has_firmware(void)
{
    return s_ota.update_available && s_ota.firmware_needs_update;
}

bool http_ota_manager_has_spiffs(void)
{
    return s_ota.update_available && s_ota.spiffs_needs_update;
}

bool http_ota_manager_is_update_available(void)
{
    return s_ota.update_available;
}

bool http_ota_manager_is_busy(void)
{
    return s_ota.busy;
}

bool http_ota_manager_has_error(void)
{
    return s_ota.error;
}

int http_ota_manager_get_progress(void)
{
    return s_ota.progress;
}

const char *http_ota_manager_get_status_msg(void)
{
    return s_ota.status_msg;
}

/* ========================================== 检查远程更新 ============================================= */

static char s_ota_resp_buf[2048];
static int  s_ota_resp_len = 0;

/**
 * @brief HTTP 客户端事件回调
 *
 * 在 HTTP_EVENT_ON_FINISH 中解析 version.json 并执行版本比较。
 */
static esp_err_t http_client_event(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (s_ota_resp_len + evt->data_len < (int)sizeof(s_ota_resp_buf)) {
            memcpy(s_ota_resp_buf + s_ota_resp_len, evt->data, evt->data_len);
            s_ota_resp_len += evt->data_len;
        }
        break;

    case HTTP_EVENT_ON_FINISH:
        s_ota_resp_buf[s_ota_resp_len] = '\0';

        cJSON *root = cJSON_Parse(s_ota_resp_buf);
        if (!root) {
            ESP_LOGE(TAG, "version.json parse failed: %s", s_ota_resp_buf);
            break;
        }

        cJSON *v   = cJSON_GetObjectItem(root, "version");
        cJSON *u   = cJSON_GetObjectItem(root, "firmware_url");
        cJSON *d   = cJSON_GetObjectItem(root, "description");
        cJSON *su  = cJSON_GetObjectItem(root, "spiffs_url");
        cJSON *fw_en = cJSON_GetObjectItem(root, "firmware_enabled");
        cJSON *sp_en = cJSON_GetObjectItem(root, "spiffs_enabled");
        cJSON *force = cJSON_GetObjectItem(root, "force");

        if (!v || !cJSON_IsString(v)) {
            ESP_LOGE(TAG, "version.json missing 'version'");
            cJSON_Delete(root);
            break;
        }

        bool has_url    = (u && cJSON_IsString(u) && u->valuestring[0]);
        bool has_spiffs = (su && cJSON_IsString(su) && su->valuestring[0]);

        bool fw_ok = has_url;
        if (fw_en && cJSON_IsBool(fw_en) && !cJSON_IsTrue(fw_en)) {
            fw_ok = false;
        }

        bool sp_ok = has_spiffs;
        if (sp_en && cJSON_IsBool(sp_en) && !cJSON_IsTrue(sp_en)) {
            sp_ok = false;
        }

        if (!fw_ok && !sp_ok) {
            ESP_LOGE(TAG, "version.json: firmware/spiffs both disabled or no URLs");
            cJSON_Delete(root);
            break;
        }

        const char *remote_ver = v->valuestring;

        strncpy(s_ota.new_version, remote_ver, sizeof(s_ota.new_version) - 1);

        if (fw_ok) {
            strncpy(s_ota.new_url, u->valuestring, sizeof(s_ota.new_url) - 1);
        } else {
            s_ota.new_url[0] = '\0';
        }
        if (sp_ok) {
            strncpy(s_ota.new_spiffs_url, su->valuestring,
                    sizeof(s_ota.new_spiffs_url) - 1);
        } else {
            s_ota.new_spiffs_url[0] = '\0';
        }
        if (d && cJSON_IsString(d)) {
            strncpy(s_ota.new_desc, d->valuestring, sizeof(s_ota.new_desc) - 1);
        }

        bool force_update = (force && cJSON_IsBool(force) && cJSON_IsTrue(force));

        bool fw_needs_update;
        bool sp_needs_update;

        if (force_update) {
            fw_needs_update = fw_ok;
            sp_needs_update = sp_ok;
            ESP_LOGI(TAG, "force update enabled, skipping version comparison");
        } else {
            int cmp = version_compare(s_ota.running_version, remote_ver);
            fw_needs_update = fw_ok && (cmp < 0);
            sp_needs_update = sp_ok && (cmp < 0);
        }

        s_ota.firmware_needs_update = fw_needs_update;
        s_ota.spiffs_needs_update   = sp_needs_update;

        if (fw_needs_update || sp_needs_update) {
            s_ota.update_available = true;
            ESP_LOGI(TAG, "update available: %s -> %s%s, desc=%s",
                     s_ota.running_version, s_ota.new_version,
                     force_update ? " [FORCE]" : "", s_ota.new_desc);
            notify(1, "Update available");
        } else {
            s_ota.update_available = false;
            ESP_LOGI(TAG, "already latest: %s", s_ota.running_version);
            notify(0, "Already latest");
        }

        cJSON_Delete(root);
        break;

    default:
        break;
    }
    return ESP_OK;
}

/** @brief 参见 http_ota_manager.h */
esp_err_t http_ota_manager_check(void)
{
    if (!s_ota.version_url[0]) {
        ESP_LOGW(TAG, "version_url not set, skip check");
        return ESP_FAIL;
    }
    if (s_ota.busy) {
        ESP_LOGW(TAG, "ota busy, skip check");
        return ESP_ERR_INVALID_STATE;
    }

    s_ota.update_available        = false;
    s_ota.error                   = false;
    s_ota.firmware_needs_update   = false;
    s_ota.spiffs_needs_update     = false;
    s_ota.status_msg[0]           = '\0';
    s_ota.new_version[0]          = '\0';
    s_ota.new_url[0]              = '\0';
    s_ota.new_spiffs_url[0]       = '\0';
    s_ota.new_desc[0]             = '\0';

    ESP_LOGI(TAG, "checking version from OTA server");

    s_ota_resp_len = 0;
    esp_http_client_config_t cfg = {
        .url            = s_ota.version_url,
        .method         = HTTP_METHOD_GET,
        .timeout_ms     = 10000,
        .event_handler  = http_client_event,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "http client init failed");
        return ESP_FAIL;
    }

    esp_err_t ret = esp_http_client_perform(client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "http request failed: %s", esp_err_to_name(ret));
    }

    esp_http_client_cleanup(client);
    return ret;
}

/* ========================================== 通用分区下载工具 ============================================= */

/**
 * @brief 下载数据并直接写入指定分区
 *
 * 先擦除分区再下载，不做预检查以避免两次 TLS 握手。
 *
 * @param url       下载 URL
 * @param part      目标分区
 * @param phase_msg 进度描述
 * @return
 *      - ESP_OK               下载并写入成功
 *      - ESP_ERR_INVALID_STATE 用户取消
 *      - 其他                  网络或分区写入失败
 */
static esp_err_t download_to_partition(const char *url,
                                        const esp_partition_t *part,
                                        const char *phase_msg)
{
    strncpy(s_ota.status_msg, phase_msg, sizeof(s_ota.status_msg) - 1);
    notify(2, phase_msg);

    esp_err_t ret = esp_partition_erase_range(part, 0, part->size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "erase partition failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_http_client_config_t cfg = {
        .url            = url,
        .method         = HTTP_METHOD_GET,
        .timeout_ms     = 30000,
        .event_handler  = NULL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "http client init failed");
        return ESP_FAIL;
    }

    ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "http open failed: %s", esp_err_to_name(ret));
        esp_http_client_cleanup(client);
        return ret;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int total_written  = 0;

    char buf[1024];
    int  read_len;
    int  last_pct = -1;
    while ((read_len = esp_http_client_read(client, buf, sizeof(buf))) > 0) {
        if (s_ota.cancel_requested) {
            ESP_LOGI(TAG, "download cancelled by user");
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_ERR_INVALID_STATE;
        }

        ret = esp_partition_write(part, total_written, buf, read_len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "partition write failed: %s", esp_err_to_name(ret));
            break;
        }
        total_written += read_len;

        if (content_length > 0) {
            int pct = (total_written * 100) / content_length;
            if (pct != last_pct) {
                last_pct       = pct;
                s_ota.progress = pct;
                ESP_LOGI(TAG, "%s progress %d%%", phase_msg, pct);
            }
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (read_len < 0) {
        ESP_LOGE(TAG, "download error");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/* ========================================== 执行更新 ============================================= */

/**
 * @brief 执行 OTA 更新
 *
 * 按顺序执行固件 OTA 和 SPIFFS A/B 更新，完成后自动重启。
 */
static esp_err_t http_ota_manager_do_update(void)
{
    if (!s_ota.update_available) {
        ESP_LOGW(TAG, "no update available");
        return ESP_FAIL;
    }

    bool has_firmware = s_ota.firmware_needs_update;
    bool has_spiffs   = s_ota.spiffs_needs_update;

    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "========== OTA UPDATE START ==========");
    ESP_LOGI(TAG, "Update plan: firmware=%s spiffs=%s",
             has_firmware ? "YES" : "NO",
             has_spiffs ? "YES" : "NO");

    /* --- 第 1 步：固件更新 --- */
    if (has_firmware) {
        const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
        if (!update_partition) {
            ESP_LOGE(TAG, "no OTA partition found");
            s_ota.busy = false;
            notify(-1, "No OTA partition");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Running from: %s (0x%x) | Downloading to: %s (0x%x) | After reboot will boot: %s",
                 running_partition ? running_partition->label : "?",
                 running_partition ? running_partition->address : 0,
                 update_partition->label,
                 update_partition->address,
                 update_partition->label);

        esp_ota_handle_t ota_handle;
        ret = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(ret));
            s_ota.busy = false;
            notify(-1, "OTA begin failed");
            return ret;
        }

        esp_http_client_config_t cfg = {
            .url            = s_ota.new_url,
            .method         = HTTP_METHOD_GET,
            .timeout_ms     = 30000,
            .event_handler  = NULL,
            .crt_bundle_attach = esp_crt_bundle_attach,
        };

        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (!client) {
            ESP_LOGE(TAG, "http client init failed");
            esp_ota_abort(ota_handle);
            s_ota.busy = false;
            notify(-1, "HTTP init failed");
            return ESP_FAIL;
        }

        ret = esp_http_client_open(client, 0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "http open failed: %s", esp_err_to_name(ret));
            esp_http_client_cleanup(client);
            esp_ota_abort(ota_handle);
            s_ota.busy = false;
            notify(-1, "HTTP open failed");
            return ret;
        }

        int  content_length = esp_http_client_fetch_headers(client);
        int  total_read     = 0;
        char buf[2048];
        int  read_len;
        int  last_pct = -1;
        while ((read_len = esp_http_client_read(client, buf, sizeof(buf))) > 0) {
            if (s_ota.cancel_requested) {
                ESP_LOGI(TAG, "download cancelled by user");
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                esp_ota_abort(ota_handle);
                s_ota.busy = false;
                notify(-1, "Cancelled by user");
                return ESP_FAIL;
            }
            ret = esp_ota_write(ota_handle, buf, read_len);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(ret));
                break;
            }
            total_read += read_len;

            if (content_length > 0) {
                int pct = (total_read * 100) / content_length;
                if (pct != last_pct) {
                    last_pct       = pct;
                    s_ota.progress = pct;
                    ESP_LOGI(TAG, "firmware download progress %d%%", pct);
                }
            }
        }

        esp_http_client_close(client);
        esp_http_client_cleanup(client);

        if (read_len < 0) {
            ESP_LOGE(TAG, "download error");
            esp_ota_abort(ota_handle);
            s_ota.busy = false;
            notify(-1, "Download error");
            return ESP_FAIL;
        }

        ret = esp_ota_end(ota_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(ret));
            s_ota.busy = false;
            notify(-1, "OTA end failed");
            return ret;
        }

        ret = esp_ota_set_boot_partition(update_partition);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s",
                     esp_err_to_name(ret));
            s_ota.busy = false;
            notify(-1, "Set boot partition failed");
            return ret;
        }

        ESP_LOGI(TAG, "firmware OTA done");
    } else {
        ESP_LOGI(TAG, "Running from: %s (0x%x) | SPIFFS-only update",
                 running_partition ? running_partition->label : "?",
                 running_partition ? running_partition->address : 0);
    }

    /* --- 第 2 步：SPIFFS 更新（A/B 分区切换）--- */
    if (has_spiffs) {
        int active_idx = spiffs_ota_get_active_idx();
        int target_idx = (active_idx == 0) ? 1 : 0;

        const char *active_label = (active_idx == 0) ? "spiffs_0" : "spiffs_1";
        const char *target_label = (target_idx == 0) ? "spiffs_0" : "spiffs_1";

        ESP_LOGI(TAG, "starting SPIFFS update");
        ESP_LOGI(TAG, "SPIFFS from: %s | Downloading to: %s | After reboot will boot: %s",
                 active_label, target_label, target_label);

        const esp_partition_t *spiffs_part = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA,
            ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
            target_label);
        if (!spiffs_part) {
            ESP_LOGE(TAG, "no SPIFFS partition %s found", target_label);
            notify(-1, "No SPIFFS partition");
        } else {
            s_ota.progress       = 0;
            if (!has_firmware) {
                strncpy(s_ota.status_msg, "spiffs",
                        sizeof(s_ota.status_msg) - 1);
            }
            ret = download_to_partition(s_ota.new_spiffs_url, spiffs_part,
                                        "spiffs");

            if (ret == ESP_ERR_INVALID_STATE) {
                ESP_LOGI(TAG, "SPIFFS update cancelled by user");
                s_ota.busy = false;
                notify(-1, "Cancelled by user");
                return ESP_FAIL;
            }
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "SPIFFS update failed: %s", esp_err_to_name(ret));
                s_ota.busy = false;
                notify(-1, "Filesystem update failed");
                return ESP_FAIL;
            }

            spiffs_ota_set_active(target_idx);

            ESP_LOGI(TAG, "SPIFFS update done, now active: %s", target_label);
            s_ota.progress = 100;
        }
    }

    ESP_LOGI(TAG, "OTA fully done, rebooting in 1 second...");
    notify(3, "Update success, rebooting...");

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}