/**
 * @file    led_indicator.h
 * @brief   WS2812 RGB LED 状态指示器接口
 *
 * 通过 led_strip 组件（RMT 后端）驱动单颗 WS2812，
 * 以颜色与闪烁模式反映设备运行状态。
 *
 * @date    2026-05-18
 * @author  Mr.Tang
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WS2812 RGB LED 数据引脚（通过 idf.py menuconfig 调整）
 */
#define LED_GPIO_PIN_INIT  CONFIG_LED_GPIO_PIN

/* ================================================================
 * 状态枚举
 * ================================================================ */

/**
 * @brief LED 运行状态
 */
typedef enum {
    LED_STATE_BOOTING,          /**< 启动中（灯灭，等待配置） */
    LED_STATE_AP_MODE,          /**< AP 配网模式（蓝色慢闪） */
    LED_STATE_CONNECTING,       /**< WiFi 连接中（绿色慢闪） */
    LED_STATE_CONNECTED,        /**< WiFi 已连接（绿色常亮） */
    LED_STATE_CONNECT_FAILED,   /**< WiFi 连接失败（红灯常亮） */
    LED_STATE_FILE_OPERATION,   /**< 文件传输中（白色快闪） */
    LED_STATE_OTA_UPDATING,     /**< OTA 升级中（紫色快闪） */
    LED_STATE_ERROR,            /**< 通用错误（红灯常亮 2 秒后自动恢复绿色） */
} led_state_t;

/**
 * @brief 文件传输事件类型
 */
typedef enum {
    LED_FILE_STARTED,   /**< 文件传输开始 → 白色快闪 */
    LED_FILE_FINISHED,  /**< 文件传输正常完成 → 恢复绿色 */
    LED_FILE_FAILED,    /**< 文件传输失败 → 红灯 2 秒后恢复绿色 */
} led_file_event_t;

/**
 * @brief OTA 升级事件类型
 */
typedef enum {
    LED_OTA_STARTED,    /**< OTA 开始 → 紫色快闪 */
    LED_OTA_ERROR,      /**< OTA 错误 → 红灯 2 秒（每轮仅触发一次） */
    LED_OTA_CANCELLED,  /**< OTA 取消 → 红灯 2 秒 */
    LED_OTA_FINISHED,   /**< OTA 完成 → 恢复绿色常亮 */
} led_ota_event_t;

/**
 * @brief WiFi 状态事件类型
 */
typedef enum {
    LED_WIFI_CONNECTING,        /**< WiFi 连接中 → 绿色慢闪 */
    LED_WIFI_CONNECTED,         /**< WiFi 已连接 → 绿色常亮 */
    LED_WIFI_CONNECT_FAILED,    /**< WiFi 连接失败 → 红灯常亮 */
    LED_WIFI_DISCONNECTED,      /**< WiFi 断开 → 蓝色慢闪（配网模式） */
} led_wifi_event_t;

/* ================================================================
 * 公共 API
 * ================================================================ */

/**
 * @brief LED 指示器初始化
 *
 * 创建 led_strip 实例（RMT 后端）、互斥锁与闪烁定时器。
 * 初始化后 LED 处于熄灭状态，后续由事件通知函数驱动。
 *
 * @param gpio_pin WS2812 数据引脚编号
 * @return
 *      - ESP_OK      初始化成功
 *      - ESP_ERR_NO_MEM 内存不足
 */
esp_err_t led_indicator_init(int gpio_pin);

/**
 * @brief 切换 LED 运行状态
 *
 * 立即切换颜色与闪烁模式。内部通过互斥锁保护 RMT 访问，
 * 调用方在任务上下文中可阻塞等待（最长 500ms）。
 *
 * @param state 目标运行状态
 */
void led_indicator_set_state(led_state_t state);

/**
 * @brief 设置 LED 总开关
 *
 * 关闭时物理熄灭并忽略状态切换（内部仍记录当前状态），
 * 重新开启时恢复为当前记录的状态。
 *
 * @param enabled true=开启，false=关闭
 */
void led_indicator_set_enabled(bool enabled);

/**
 * @brief 查询 LED 总开关状态
 *
 * @return true=已开启，false=已关闭
 */
bool led_indicator_is_enabled(void);

/**
 * @brief 文件传输事件通知
 *
 * 内部维护引用计数器以支持并发传输。
 * 首个传输开始时切换白色快闪，全部传输结束时根据是否出错决定恢复绿色或亮红灯。
 *
 * @param event 文件传输事件
 */
void led_indicator_file_event(led_file_event_t event);

/**
 * @brief OTA 升级事件通知
 *
 * 内部处理防重复触发：同轮升级中 OTA_ERROR 仅亮一次红灯。
 *
 * @param event OTA 升级事件
 */
void led_indicator_ota_event(led_ota_event_t event);

/**
 * @brief WiFi 状态事件通知
 *
 * 根据 WiFi 连接/断开/失败事件切换对应灯色。
 *
 * @param event WiFi 状态事件
 */
void led_indicator_wifi_event(led_wifi_event_t event);

/**
 * @brief 从 NVS 加载 LED 配置
 *
 * 因 LED 初始化在 NVS 初始化之前完成，此函数用于延迟加载持久化配置。
 * 须在 NVS 初始化后调用（wifi_manage_init 内部已完成 NVS 初始化）。
 */
void led_indicator_load_nvs_config(void);

#ifdef __cplusplus
}
#endif
