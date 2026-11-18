/**
 * @file    event_defs.h
 * @brief   系统事件 ID 定义
 *
 * 所有模块共享的事件常量，定义在 event_bus 组件中以便各组件引用。
 * 添加新的事件类型只需在此 enum 中追加，无需修改 event_bus 组件代码。
 *
 * @date    2026-07-24
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================== 系统事件 ID ============================================= */

/** @brief 系统事件 ID */
typedef enum {
    EVENT_NONE = 0,

    /* ---- WiFi 事件 ---- */
    EVENT_WIFI_CONNECTING,      /**< payload: NULL（用户从网页发起连接） */
    EVENT_WIFI_CONNECTED,
    EVENT_WIFI_DISCONNECTED,
    EVENT_WIFI_CONNECT_FAILED,

    /* ---- USB 事件 ---- */
    EVENT_USB_MODE_CHANGED,       /**< payload: int new_mode（模式已切换完成） */
    EVENT_USB_MODE_PENDING,       /**< payload: int pending_mode（即将切换，预通知） */
    EVENT_USB_SWITCH_REQUEST,     /**< payload: int target_mode（按钮请求切换） */
    EVENT_NCM_EXIT,               /**< payload: NULL（NCM 模式退出，WiFi 需断开重连） */

    /* ---- 文件事件 ---- */
    EVENT_FILE_UPLOAD_STARTED,
    EVENT_FILE_UPLOAD_FINISHED,
    EVENT_FILE_UPLOAD_FAILED,
    EVENT_FILE_DOWNLOAD_STARTED,
    EVENT_FILE_DOWNLOAD_FINISHED,
    EVENT_FILE_DOWNLOAD_FAILED,

    /* ---- OTA 事件 ---- */
    EVENT_OTA_STARTED,
    EVENT_OTA_FINISHED,
    EVENT_OTA_ERROR,

    /* ---- 系统事件 ---- */
    EVENT_LED_SET_ENABLED,        /**< payload: bool enabled */

    EVENT_MAX
} superdongle_event_id_t;

#ifdef __cplusplus
}
#endif
