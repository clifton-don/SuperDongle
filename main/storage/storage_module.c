/**
 * @file    storage_module.c
 * @brief   文件系统存储实现
 *
 * 两类存储功能：
 *   1. SPIFFS - 内部 Flash 上的文件系统，存储网页文件（index.html 等）
 *      使用 A/B 双分区设计（spiffs_0 / spiffs_1），通过 NVS 记录活跃分区。
 *   2. SD 卡 FATFS - 外部 SD 卡文件系统，用于网盘功能。
 *
 * WiFi 配置 NVS 持久化已拆分至 wifi_storage.c。
 *
 * SPIFFS A/B 说明：
 * - 本模块中提供了 spiffs_ota_* 函数族，用于读写 NVS 中的 SPIFFS A/B 信息。
 * - http_ota_manager.c 中也有完全相同的函数（组件隔离要求）。
 *
 * @date    2026-05-18
 * @author  Mr.Tang
 */

#include "storage_module.h"

#include <sys/unistd.h>
#include <sys/stat.h>

#include "esp_spiffs.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_heap_caps.h"

#include "ff.h"
#include "diskio.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

static const char *TAG = "[storage_module]";

/* ================================================================
 * 配置宏
 * ================================================================ */

#define SPIFFS_OTA_NVS "spiffs_ota"

/* 引脚配置（通过 idf.py menuconfig → SuperDongle Hardware Configuration 调整） */
#define SDMMC_CLK_PIN   CONFIG_SDMMC_CLK_PIN
#define SDMMC_CMD_PIN   CONFIG_SDMMC_CMD_PIN
#define SDMMC_D0_PIN    CONFIG_SDMMC_D0_PIN
#define SDMMC_D1_PIN    CONFIG_SDMMC_D1_PIN
#define SDMMC_D2_PIN    CONFIG_SDMMC_D2_PIN
#define SDMMC_D3_PIN    CONFIG_SDMMC_D3_PIN
#define USE_4BIT_MODE   1  /**< 1 = 4 位高速模式，0 = 1 位模式 */

/* ================================================================
 * 模块级静态变量
 * ================================================================ */

static sdmmc_card_t          *s_sdcard       = NULL;  /**< SD 卡句柄 */

/* ================================================================
 * FATFS + SD 卡
 * ================================================================ */

/**
 * @brief 初始化 FATFS SDMMC （高速模式）
 *
 * 配置 SDMMC 主机、引脚、挂载 FATFS。支持 4 位和 1 位数据总线模式。
 *
 * @return
 *      - ESP_OK     挂载成功
 *      - 其他       esp_vfs_fat_sdmmc_mount 错误码
 */
esp_err_t storage_fatfs_sdcard_init(void)
{
    ESP_LOGI(TAG, "Initializing FATFS (SDMMC High Speed)...");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = SDMMC_CLK_PIN;
    slot_config.cmd = SDMMC_CMD_PIN;
    slot_config.d0 = SDMMC_D0_PIN;

#if CONFIG_SDMMC_USE_CD_PIN
    slot_config.gpio_cd = CONFIG_SDMMC_CD_PIN_GPIO;
#endif

#if USE_4BIT_MODE
    slot_config.width = 4;
    slot_config.d1 = SDMMC_D1_PIN;
    slot_config.d2 = SDMMC_D2_PIN;
    slot_config.d3 = SDMMC_D3_PIN;
#else
    slot_config.width = 1;
#endif

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = true,
        .max_files = 10,
        .allocation_unit_size = 16 * 1024,
    };

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(
        STORAGE_FATFS_MOUNT_POINT,
        &host,
        &slot_config,
        &mount_cfg,
        &s_sdcard
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FATFS mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    sdmmc_card_print_info(stdout, s_sdcard);

    ESP_LOGI(TAG, "FATFS mounted at %s (SDMMC High Speed)", STORAGE_FATFS_MOUNT_POINT);
    return ESP_OK;
}

/**
 * @brief 获取 SD 卡句柄
 *
 * @return sdmmc_card_t* 句柄，未初始化时返回 NULL
 */
sdmmc_card_t* storage_fatfs_get_sdcard(void)
{
    return s_sdcard;
}

/* ================================================================
 * SPIFFS 文件系统 + A/B 分区管理
 *
 * SPIFFS A/B 双分区设计：
 *   设备上有两个 SPIFFS 分区：spiffs_0 和 spiffs_1，大小各 1000K。
 *   - 当前活跃的分区由 NVS（spiffs_ota 命名空间）中的 "active" 值决定。
 *   - OTA 更新时写入非活跃分区，成功后再切换活跃标志。
 *   - PlatformIO 的 uploadfs 命令默认写入 spiffs_1 分区。
 *
 *   storage_module 属于主项目，http_ota_manager 是独立组件，
 *   两者均需读写 SPIFFS NVS 信息但互相不可依赖。
 * ================================================================ */

/**
 * @brief 从 NVS 读取 SPIFFS 活跃分区索引
 *
 * 默认值：1（spiffs_1），因为 uploadfs 默认写入 spiffs_1。
 *
 * @return 0=spiffs_0, 1=spiffs_1
 */
int storage_spiffs_get_active_idx(void)
{
    nvs_handle_t handle;
    uint8_t active = 0;
    esp_err_t err = nvs_open(SPIFFS_OTA_NVS, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        err = nvs_get_u8(handle, "active", &active);
        nvs_close(handle);
    }
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return 1;
    }
    return (active == 1) ? 1 : 0;
}

/**
 * @brief 获取当前活跃 SPIFFS 分区的标签名
 *
 * @return "spiffs_0" 或 "spiffs_1"
 */
const char* storage_spiffs_get_active_label(void)
{
    return storage_spiffs_get_active_idx() == 0 ? "spiffs_0" : "spiffs_1";
}

/**
 * @brief 设置 SPIFFS 活跃分区索引并写入 NVS
 *
 * @param idx 0=spiffs_0, 1=spiffs_1
 * @return
 *      - ESP_OK     写入成功
 *      - 其他       nvs 操作错误码
 */
esp_err_t storage_spiffs_set_active(int idx)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(SPIFFS_OTA_NVS, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return ret;

    uint8_t val = (idx == 1) ? 1 : 0;
    ret = nvs_set_u8(handle, "active", val);
    if (ret == ESP_OK) ret = nvs_commit(handle);
    nvs_close(handle);
    return ret;
}

/**
 * @brief 初始化并挂载 SPIFFS 虚拟文件系统
 *
 * 根据 NVS 记录的活跃分区（spiffs_0 或 spiffs_1）挂载。
 * 如果 A/B 分区不存在，则回退到旧的 "spiffs" 分区名（向后兼容）。
 *
 * @return
 *      - ESP_OK     挂载成功
 *      - 其他       esp_vfs_spiffs_register 错误码
 */
esp_err_t storage_spiffs_init(void)
{
    const char *label = storage_spiffs_get_active_label();
    ESP_LOGI(TAG, "Attempting to mount SPIFFS from %s", label);

    esp_vfs_spiffs_conf_t st_spiffs_config = {
        .base_path = STORAGE_SPIFFS_MOUNT_POINT,
        .partition_label = label,
        .max_files = 4,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&st_spiffs_config);
    if (ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "%s not found, falling back to legacy \"spiffs\" partition", label);
        st_spiffs_config.partition_label = "spiffs";
        ret = esp_vfs_spiffs_register(&st_spiffs_config);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spiffs register failed on %s: %s", st_spiffs_config.partition_label, esp_err_to_name(ret));
        return ret;
    }

    size_t u32_total_bytes = 0;
    size_t u32_used_bytes = 0;
    storage_module_spiffs_get_usage_statistics(&u32_total_bytes, &u32_used_bytes);

    ESP_LOGI(TAG, "SPIFFS VFS mounted from %s | used/total: %u/%u bytes",
             st_spiffs_config.partition_label, (uint32_t)u32_used_bytes, (uint32_t)u32_total_bytes);

    return ESP_OK;
}

/**
 * @brief 获取当前 SPIFFS 分区的总大小和已用空间
 *
 * @param pu32_total_bytes 输出总字节数
 * @param pu32_used_bytes  输出已用字节数
 */
void storage_module_spiffs_get_usage_statistics(size_t *pu32_total_bytes, size_t *pu32_used_bytes)
{
    const char *label = storage_spiffs_get_active_label();
    esp_err_t e_result = esp_spiffs_info(label, pu32_total_bytes, pu32_used_bytes);
    if (e_result != ESP_OK) {
        ESP_LOGE(TAG, "Error fetching partition(%s) info: %s", label, esp_err_to_name(e_result));
    }
}

/**
 * @brief 检查 SPIFFS 中指定文件是否存在
 *
 * 使用 stat() 而非 fopen()，不占用文件句柄，效率更高。
 *
 * @param pc_file_path 文件路径
 * @return true=文件存在，false=不存在或参数错误
 */
bool storage_module_spiffs_file_exists(const char *pc_file_path)
{
    if (pc_file_path == NULL) {
        ESP_LOGW(TAG, "File existence check called with NULL pointer.");
        return false;
    }

    struct stat st_file_stat;
    return (stat(pc_file_path, &st_file_stat) == 0);
}