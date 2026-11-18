/**
 * @file    main.c
 * @brief   SuperDongle 主入口
 *
 * 初始化顺序：
 *   1. 事件总线 + OTA 监听注册
 *   2. LED 指示器（WS2812，内部加载 NVS 配置）
 *   3. WiFi 管理（AP + STA）
 *   4. mDNS 服务（默认域名 superdongle.local）
 *   5. Web 模块（HTTP 服务 + SPIFFS，各业务路由在内部自动注册）
 *   6. USB 多功能管理（MSC + HID + NET，统一管理）
 *   7. 物理按键管理（事件总线驱动）
 *   8. 内存监控任务（调试用）
 *
 * OTA version.json 地址在 components/ota_manager/include/http_ota_manager.h 中配置。
 *
 * @date    2026-05-18
 * @author  Mr.Tang
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_heap_caps.h"

#include "wifi_manager.h"
#include "usb_manager.h"
#include "button_manager.h"
#include "led_indicator.h"
#include "web_module.h"
#include "http_ota_manager.h"
#include "mdns_service.h"
#include "event_bus.h"

static const char *TAG = "[main]";

/** @brief 内存监控任务：每 10 秒打印一次堆内存信息（调试用） */
static void memory_monitor_task(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t min_heap  = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t psram_min  = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
        ESP_LOGI("memory", "Internal: free=%uKB min=%uKB | PSRAM: free=%uKB min=%uKB",
                 (unsigned)(free_heap / 1024), (unsigned)(min_heap / 1024),
                 (unsigned)(psram_free / 1024), (unsigned)(psram_min / 1024));
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/* ================================================================
 * 主入口
 * ================================================================ */

/** @brief 系统主入口 */
void app_main()
{
    ESP_LOGI(TAG, "#####################################program start here########################################");
    ESP_LOGI(TAG, "Firmware version: %s", CONFIG_APP_PROJECT_VER);
    const esp_partition_t *boot_part = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "Booted from partition: %s (0x%x)",
             boot_part ? boot_part->label : "unknown",
             boot_part ? boot_part->address : 0);

    /* 第1步：初始化事件总线 */
    event_bus_init();

    /* 第2步：注册 OTA 自动更新监听（WiFi 连接后触发版本检查） */
    http_ota_manager_auto_init();

    /* 第3步：初始化 LED（引脚定义在 led_indicator.h 中） */
    led_indicator_init(LED_GPIO_PIN_INIT);

    /* 第4步：初始化 WiFi，事件通过 event_bus 分发 */
    wifi_manage_init(NULL);

    /* 第5步：初始化 mDNS 服务（默认域名 superdongle.local，可通过网页修改） */
    mdns_service_init();

    /* 第6步：初始化 Web 模块（HTTP 服务 + SPIFFS，各业务路由自动注册） */
    web_module_init(wifi_manage_get_web_port());

    /* 第7步：初始化 USB 多功能管理模块 */
    ESP_LOGI(TAG, "Initializing USB manager...");
    usb_manager_init(&(usb_manager_config_t){.default_mode = USB_MODE_MSC_ONLY});

    /* 第8步：初始化物理按键管理（通过事件总线驱动） */
    ESP_LOGI(TAG, "Initializing button manager...");
    button_manager_init();

    /* 第9步：创建内存监控任务（测试用） */
    xTaskCreate(memory_monitor_task, "memory_monitor", 4096, NULL, 5, NULL);
}