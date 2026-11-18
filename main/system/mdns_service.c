/**
 * @file    mdns_service.c
 * @brief   mDNS 服务模块实现
 */

#include "mdns_service.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "mdns.h"

static const char *TAG = "[mdns]";

/** @brief 默认 mDNS 主机名 */
#define MDNS_DEFAULT_HOSTNAME "superdongle"
#define MDNS_DEFAULT_INSTANCE "SuperDongle Device"

/** @brief 静态缓冲区，存放当前主机名 */
static char s_hostname[32] = {0};

/* ========================================== 公共 API ============================================= */

esp_err_t mdns_service_init(void)
{
    /* ---- 从 NVS 读取已保存的主机名 ---- */
    {
        char          saved_hostname[32] = {0};
        size_t        len = sizeof(saved_hostname);
        nvs_handle_t  nvs_h;

        if (nvs_open("superdongle", NVS_READONLY, &nvs_h) == ESP_OK) {
            if (nvs_get_str(nvs_h, "mdns_name", saved_hostname, &len) == ESP_OK) {
                strncpy(s_hostname, saved_hostname, sizeof(s_hostname) - 1);
                s_hostname[sizeof(s_hostname) - 1] = '\0';
            }
            nvs_close(nvs_h);
        }
    }

    /* 无保存值时使用默认值 */
    if (s_hostname[0] == '\0') {
        strncpy(s_hostname, MDNS_DEFAULT_HOSTNAME, sizeof(s_hostname) - 1);
        s_hostname[sizeof(s_hostname) - 1] = '\0';
    }

    /* ---- 初始化 mDNS ---- */
    {
        esp_err_t mdns_ret = mdns_init();
        if (mdns_ret == ESP_OK) {
            mdns_hostname_set(s_hostname);
            mdns_instance_name_set(MDNS_DEFAULT_INSTANCE);
            mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
            ESP_LOGI(TAG, "mDNS initialized: %s.local", s_hostname);
        } else {
            ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(mdns_ret));
        }
    }

    return ESP_OK;
}

const char *mdns_service_get_hostname(void)
{
    if (s_hostname[0] == '\0') {
        return MDNS_DEFAULT_HOSTNAME;
    }
    return s_hostname;
}

esp_err_t mdns_service_set_hostname(const char *hostname)
{
    if (!hostname || strlen(hostname) == 0 || strlen(hostname) > 31) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 保存到 NVS */
    nvs_handle_t nvs_h;
    esp_err_t    ret = nvs_open("superdongle", NVS_READWRITE, &nvs_h);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_set_str(nvs_h, "mdns_name", hostname);
    if (ret != ESP_OK) {
        nvs_close(nvs_h);
        return ret;
    }

    nvs_commit(nvs_h);
    nvs_close(nvs_h);

    /* 更新运行时缓存 */
    strncpy(s_hostname, hostname, sizeof(s_hostname) - 1);
    s_hostname[sizeof(s_hostname) - 1] = '\0';

    /* 立即更新 mDNS（运行时生效） */
    mdns_hostname_set(s_hostname);

    ESP_LOGI(TAG, "hostname updated: %s.local", s_hostname);
    return ESP_OK;
}
