#include "event_bus.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define MAX_HANDLER_SLOTS  64   /**< 全局最大注册数 */
#define MAX_HANDLERS_PER_EVENT 4

typedef struct {
    uint32_t         event_id;
    event_handler_t  handler;
    void            *user_ctx;
} handler_entry_t;

static handler_entry_t  s_handlers[MAX_HANDLER_SLOTS];
static uint8_t          s_handler_count = 0;
static SemaphoreHandle_t s_mutex = NULL;

esp_err_t event_bus_init(void)
{
    memset(s_handlers, 0, sizeof(s_handlers));
    s_handler_count = 0;
    s_mutex = xSemaphoreCreateMutex();
    return (s_mutex != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t event_bus_register(uint32_t event_id,
                              event_handler_t handler, void *user_ctx)
{
    if (handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_mutex) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* 检查是否已注册（防止重复注册浪费 slot） */
    for (uint8_t i = 0; i < s_handler_count; i++) {
        if (s_handlers[i].event_id == event_id &&
            s_handlers[i].handler == handler &&
            s_handlers[i].user_ctx == user_ctx) {
            xSemaphoreGive(s_mutex);
            return ESP_OK;
        }
    }

    if (s_handler_count >= MAX_HANDLER_SLOTS) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NO_MEM;
    }

    s_handlers[s_handler_count].event_id = event_id;
    s_handlers[s_handler_count].handler  = handler;
    s_handlers[s_handler_count].user_ctx = user_ctx;
    s_handler_count++;
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t event_bus_post(uint32_t event_id,
                          const void *payload, size_t len)
{
    if (!s_mutex) return ESP_ERR_INVALID_STATE;

    /* 先收集匹配的 handler */
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    handler_entry_t copy[MAX_HANDLERS_PER_EVENT];
    uint8_t count = 0;
    for (uint8_t i = 0; i < s_handler_count && count < MAX_HANDLERS_PER_EVENT; i++) {
        if (s_handlers[i].event_id == event_id) {
            copy[count++] = s_handlers[i];
        }
    }
    xSemaphoreGive(s_mutex);

    for (uint8_t i = 0; i < count; i++) {
        copy[i].handler(payload, len, copy[i].user_ctx);
    }
    return ESP_OK;
}
