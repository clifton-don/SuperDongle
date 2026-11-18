/**
 * @file    wifi_storage.c
 * @brief   WiFi 配置持久化存储实现
 *
 * 将用户成功连接的 WiFi 列表保存在 NVS 中，重启后自动重连。
 * 列表按最近成功连接优先排序，最多保存 max_wifi_num 条。
 *
 * @date    2026-07-24
 * @author  Mr.Tang
 */

#include "wifi_storage.h"

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "[wifi_storage]";

/* ================================================================
 * 模块级静态变量
 * ================================================================ */

/** @brief NVS 中存储 WiFi 列表的 key */
static const char *WIFI_LIST_KEY = "wifi_list";

static wifi_storage_config_t  s_storage_cfg;           /**< WiFi 存储配置副本 */
static bool                   s_storage_inited = false;/**< WiFi 存储初始化标志 */

/* ================================================================
 * 内部函数
 * ================================================================ */

/** @brief NVS 初始化（自动擦除修复） */
static esp_err_t wifi_storage_init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    return ret;
}

/** @brief 判断两个 wifi_config_t 是否为同一 SSID（仅比较 SSID 字段） */
static bool wifi_storage_is_same_ssid(const wifi_config_t *a, const wifi_config_t *b)
{
    return memcmp(a->sta.ssid, b->sta.ssid, sizeof(a->sta.ssid)) == 0;
}

/* ================================================================
 * 公共 API
 * ================================================================ */

esp_err_t wifi_storage_init(const wifi_storage_config_t *config)
{
    if (s_storage_inited) {
        return ESP_OK;
    }

    s_storage_cfg = (config == NULL) ? WIFI_STORAGE_DEFAULT_CONFIG() : *config;

    if (s_storage_cfg.max_wifi_num == 0) {
        s_storage_cfg.max_wifi_num = 1;
    }

    esp_err_t ret = wifi_storage_init_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_storage_inited = true;
    return ESP_OK;
}

esp_err_t wifi_storage_load_all(wifi_config_t *configs, uint8_t *count_out)
{
    if (!s_storage_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (configs == NULL || count_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *count_out = 0;

    nvs_handle_t handle;
    esp_err_t    ret = nvs_open(s_storage_cfg.nvs_namespace, NVS_READONLY, &handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open(read) failed: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t blob_size = 0;
    ret = nvs_get_blob(handle, WIFI_LIST_KEY, NULL, &blob_size);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_get_blob(size) failed: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }

    if (blob_size == 0 || (blob_size % sizeof(wifi_config_t)) != 0) {
        ESP_LOGE(TAG, "invalid blob size: %u", (unsigned int)blob_size);
        nvs_close(handle);
        return ESP_FAIL;
    }

    uint8_t max_num    = s_storage_cfg.max_wifi_num;
    uint8_t stored_num = blob_size / sizeof(wifi_config_t);
    uint8_t read_num   = (stored_num > max_num) ? max_num : stored_num;
    size_t  read_size  = read_num * sizeof(wifi_config_t);

    ret = nvs_get_blob(handle, WIFI_LIST_KEY, configs, &read_size);
    nvs_close(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_get_blob(data) failed: %s", esp_err_to_name(ret));
        return ret;
    }

    *count_out = read_num;
    return ESP_OK;
}

esp_err_t wifi_storage_on_connected(const wifi_config_t *config)
{
    if (!s_storage_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t max_num = s_storage_cfg.max_wifi_num;
    if (max_num == 0) {
        max_num = 1;
    }

    wifi_config_t *list = (wifi_config_t *)heap_caps_calloc(max_num, sizeof(wifi_config_t), MALLOC_CAP_SPIRAM);
    if (list == NULL) {
        return ESP_ERR_NO_MEM;
    }

    uint8_t count = 0;
    esp_err_t ret = wifi_storage_load_all(list, &count);
    if (ret != ESP_OK) {
        free(list);
        return ret;
    }

    int existing_index = -1;
    for (uint8_t i = 0; i < count; ++i) {
        if (wifi_storage_is_same_ssid(&list[i], config)) {
            existing_index = (int)i;
            break;
        }
    }

    if (existing_index >= 0) {
        if (existing_index > 0) {
            wifi_config_t tmp = list[existing_index];
            memmove(&list[1], &list[0], existing_index * sizeof(wifi_config_t));
            list[0] = tmp;
        }
    } else {
        if (count < max_num) {
            if (count > 0) {
                memmove(&list[1], &list[0], count * sizeof(wifi_config_t));
            }
            list[0] = *config;
            count++;
        } else {
            if (max_num > 1) {
                memmove(&list[1], &list[0], (max_num - 1) * sizeof(wifi_config_t));
            }
            list[0] = *config;
            count   = max_num;
        }
    }

    nvs_handle_t handle;
    ret = nvs_open(s_storage_cfg.nvs_namespace, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open(write) failed: %s", esp_err_to_name(ret));
        free(list);
        return ret;
    }

    size_t blob_size = count * sizeof(wifi_config_t);
    ret              = nvs_set_blob(handle, WIFI_LIST_KEY, list, blob_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_blob(write) failed: %s", esp_err_to_name(ret));
        nvs_close(handle);
        free(list);
        return ret;
    }

    ret = nvs_commit(handle);
    nvs_close(handle);
    free(list);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit(write) failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t wifi_storage_delete_by_ssid(const char *ssid)
{
    if (!s_storage_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ssid == NULL || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t max_num = s_storage_cfg.max_wifi_num;
    if (max_num == 0) {
        max_num = 1;
    }

    wifi_config_t *list = (wifi_config_t *)heap_caps_calloc(max_num, sizeof(wifi_config_t), MALLOC_CAP_SPIRAM);
    if (list == NULL) {
        return ESP_ERR_NO_MEM;
    }

    uint8_t   count = 0;
    esp_err_t ret   = wifi_storage_load_all(list, &count);
    if (ret != ESP_OK) {
        free(list);
        return ret;
    }

    if (count == 0) {
        free(list);
        return ESP_OK;
    }

    wifi_config_t target;
    memset(&target, 0, sizeof(target));
    strncpy((char *)target.sta.ssid, ssid, sizeof(target.sta.ssid) - 1);

    uint8_t write_idx = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (wifi_storage_is_same_ssid(&list[i], &target)) {
            continue;
        }
        if (write_idx != i) {
            list[write_idx] = list[i];
        }
        write_idx++;
    }

    nvs_handle_t handle;
    ret = nvs_open(s_storage_cfg.nvs_namespace, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open(delete) failed: %s", esp_err_to_name(ret));
        free(list);
        return ret;
    }

    if (write_idx == 0) {
        ret = nvs_erase_key(handle, WIFI_LIST_KEY);
        if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "nvs_erase_key failed: %s", esp_err_to_name(ret));
            nvs_close(handle);
            free(list);
            return ret;
        }
    } else {
        size_t blob_size = write_idx * sizeof(wifi_config_t);
        ret              = nvs_set_blob(handle, WIFI_LIST_KEY, list, blob_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "nvs_set_blob(delete) failed: %s", esp_err_to_name(ret));
            nvs_close(handle);
            free(list);
            return ret;
        }
    }

    ret = nvs_commit(handle);
    nvs_close(handle);
    free(list);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit(delete) failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
