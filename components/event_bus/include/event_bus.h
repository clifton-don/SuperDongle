#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 事件处理回调类型
 * @param payload   事件携带的数据（可为 NULL）
 * @param len       数据长度
 * @param user_ctx  注册时传入的上下文指针
 */
typedef void (*event_handler_t)(const void *payload, size_t len, void *user_ctx);

/**
 * @brief 初始化事件总线（线程安全）
 * @return ESP_OK / ESP_ERR_NO_MEM
 */
esp_err_t event_bus_init(void);

/**
 * @brief 注册事件处理器
 * @param event_id  要监听的事件 ID（由外部定义）
 * @param handler   回调函数
 * @param user_ctx  用户上下文（传 NULL 也可以）
 * @return ESP_OK / ESP_ERR_NO_MEM / ESP_ERR_INVALID_ARG
 */
esp_err_t event_bus_register(uint32_t event_id,
                              event_handler_t handler,
                              void *user_ctx);

/**
 * @brief 发布事件（线程安全，回调在互斥锁外执行防止死锁）
 * @param event_id  事件 ID
 * @param payload   事件数据（可为 NULL）
 * @param len       数据长度
 * @return ESP_OK / ESP_ERR_INVALID_ARG
 */
esp_err_t event_bus_post(uint32_t event_id,
                          const void *payload,
                          size_t len);

#ifdef __cplusplus
}
#endif
