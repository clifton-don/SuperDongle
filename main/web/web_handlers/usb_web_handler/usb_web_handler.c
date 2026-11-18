/**
 * @file    usb_web_handler.c
 * @brief   USB/HID Web handler 实现
 *
 * 提供 USB 模式切换、HID 键鼠控制、WebSocket 二进制帧、粘贴打字等 API。
 *
 * @date    2026-07-26
 * @author  Mr.Tang
 */

#include "usb_web_handler.h"
#include "web_module.h"

#include "usb_manager.h"
#include "usb_hid_receiver.h"
#include "usb_hid_bridge.h"
#include "storage_module.h"

#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "usb_web";

/* ========================================== USB 模式切换 API ============================================= */

/**
 * @brief GET /api/usb/status
 *
 * 返回当前 USB 运行模式。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t web_module_usb_status_handler(httpd_req_t *req)
{
    int mode = usb_manager_get_mode();
    int pending = usb_manager_get_pending_mode();
    const char *name = usb_manager_mode_to_string(mode);
    char json[96];
    snprintf(json, sizeof(json),
             "{\"mode\":%d,\"name\":\"%s\",\"pending_mode\":%d}",
             mode, name, pending);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

/**
 * @brief NCM 延时切换回调
 */
static void ncm_delayed_switch_cb(void *arg)
{
    usb_manager_set_mode(WEB_USB_MODE_NCM);
}

/**
 * @brief POST /api/usb/mode
 *
 * 切换 USB 运行模式。Body: {"mode": 0} (MSC) 或 {"mode": 1} (HID)
 * NCM(2) 先设 pending_mode，esp_timer 延时后再真正切换（不阻塞 HTTP 任务）
 */
static esp_err_t web_module_usb_mode_handler(httpd_req_t *req)
{
    char buf[64] = {0};
    int received = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_OK;
    }
    buf[received] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    cJSON *item = cJSON_GetObjectItem(root, "mode");
    if (!item || !cJSON_IsNumber(item)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid 'mode'");
        return ESP_OK;
    }

    int mode_val = item->valueint;
    cJSON_Delete(root);

    if (mode_val < 0 || mode_val >= WEB_USB_MODE_COUNT) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                           "Invalid mode value (0=MSC, 1=HID, 2=NCM)");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Web API: switching USB mode to %s",
             usb_manager_mode_to_string(mode_val));

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", 9);

    if (mode_val == WEB_USB_MODE_NCM) {
        static esp_timer_handle_t ncm_timer = NULL;
        usb_manager_set_pending_mode(WEB_USB_MODE_NCM);
        if (!ncm_timer) {
            esp_timer_create(&(esp_timer_create_args_t){.callback=ncm_delayed_switch_cb,.name="ncm"}, &ncm_timer);
        }
        esp_timer_start_once(ncm_timer, 2500000);  // 2.5s: >fetchUsbMode 2s轮询，确保不漏 pending_mode
        return ESP_OK;
    }

    usb_manager_set_mode(mode_val);
    return ESP_OK;
}

/* ========================================== HID 键鼠控制 API ============================================= */

#define HID_MAX_KEYCODE 0xE7  /**< HID 按键码最大值 */

static uint8_t s_hid_key_state[(HID_MAX_KEYCODE / 8) + 1]; /**< 键盘按键状态位图，按 HID usage code 索引 */

/**
 * @brief 根据按键状态重建并发送键盘 HID 报告
 */
static void hid_keyboard_rebuild_and_send(void)
{
    uint8_t modifier = 0;
    uint8_t keycode[6] = {0};
    int     key_idx   = 0;

    for (int kc = 0; kc <= HID_MAX_KEYCODE && key_idx < 6; kc++) {
        int byte_idx = kc / 8;
        int bit_pos  = kc % 8;
        if (!(s_hid_key_state[byte_idx] & (1U << bit_pos))) {
            continue;
        }
        if (kc >= 0xE0 && kc <= 0xE7) {
            modifier |= (uint8_t)(1U << (kc - 0xE0));
        } else if (kc != 0) {
            keycode[key_idx++] = (uint8_t)kc;
        }
    }

    usb_hid_keyboard_send(modifier, keycode);
}

typedef struct {
    uint8_t *seq;
    int      len;
} paste_job_t;

static QueueHandle_t s_paste_queue = NULL;

static int s_ws_fd = -1;  /**< 当前活跃的 WS 连接 fd，新连接来了踢旧连接 */

/**
 * @brief 关闭当前 HID WebSocket 连接（模式切换/桥接停止时调用）
 */
void usb_web_close_hid_ws(void)
{
    if (s_ws_fd >= 0) {
        ESP_LOGI(TAG, "Closing HID WS fd=%d", s_ws_fd);
        httpd_sess_trigger_close(web_module_get_server_handle(), s_ws_fd);
        s_ws_fd = -1;
    }
}

/* ========================================== HID WebSocket 二进制帧端点（类UDP火忘） ============================================= */
/**
 * @brief WebSocket 二进制帧处理
 *
 * 帧格式:
 *   鼠标 [0x01, dx(i8), dy(i8), btns(u8), wheel(i8)]          5 字节
 *   键盘 [0x04, pressed(u8), code(u8)]                         3 字节
 *   粘贴 [0x05, cnt_hi, cnt_lo, code1, shift1, code2, ...]    3+N*2 字节
 */
static esp_err_t web_module_hid_ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        int new_fd = httpd_req_to_sockfd(req);
        if (s_ws_fd >= 0 && s_ws_fd != new_fd) {
            httpd_sess_trigger_close(web_module_get_server_handle(), s_ws_fd);
        }
        s_ws_fd = new_fd;
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        int fd = httpd_req_to_sockfd(req);
        if (fd == s_ws_fd) s_ws_fd = -1;
        return ret;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
        char tmp[16] = {0};
        size_t rlen = ws_pkt.len < sizeof(tmp) - 1 ? ws_pkt.len : sizeof(tmp) - 1;
        ws_pkt.payload = (uint8_t *)tmp;
        httpd_ws_recv_frame(req, &ws_pkt, rlen);
        if (strcmp(tmp, "bye_bye") == 0) {
            int fd = httpd_req_to_sockfd(req);
            if (fd == s_ws_fd) s_ws_fd = -1;
        }
        return ESP_OK;
    }

    if (ws_pkt.type != HTTPD_WS_TYPE_BINARY || ws_pkt.len < 2) return ESP_OK;

    uint8_t *buf = malloc(ws_pkt.len + 1);
    if (!buf) return ESP_ERR_NO_MEM;
    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK || !usb_hid_is_active()) { free(buf); return ret; }

    uint8_t type = buf[0];

    if (type == 0x01 && ws_pkt.len >= 5) {
        int8_t dx    = (int8_t)buf[1];
        int8_t dy    = (int8_t)buf[2];
        uint8_t btns = buf[3];
        int8_t wheel = (int8_t)buf[4];
        usb_hid_mouse_send(btns, dx, dy, wheel);
    } else if (type == 0x04 && ws_pkt.len >= 3) {
        uint8_t pressed = buf[1];
        uint8_t code    = buf[2];
        if (code > 0) {
            int bi = code / 8, bp = code % 8;
            if (pressed) {
                s_hid_key_state[bi] |= (uint8_t)(1U << bp);
            } else {
                s_hid_key_state[bi] &= (uint8_t)(~(1U << bp));
            }
            hid_keyboard_rebuild_and_send();
        }
    } else if (type == 0x05 && ws_pkt.len >= 3) {
        int count = (buf[1] << 8) | buf[2];
        if (count > 0 && count <= 512 && ws_pkt.len >= 3 + count * 2) {
            uint8_t *seq = (uint8_t *)malloc((size_t)count * 2);
            if (seq) {
                memcpy(seq, buf + 3, (size_t)count * 2);
                paste_job_t job = { .seq = seq, .len = count };
                if (xQueueSend(s_paste_queue, &job, 0) != pdTRUE) {
                    free(seq);
                }
            }
        }
    }

    free(buf);
    return ESP_OK;
}

/* ========================================== HID 粘贴打字 ============================================= */

static TaskHandle_t s_paste_task = NULL;

static void paste_task_func(void *arg)
{
    (void)arg;
    paste_job_t job;

    while (1) {
        if (xQueueReceive(s_paste_queue, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (!usb_hid_is_active()) {
            free(job.seq);
            continue;
        }

        for (int i = 0; i < job.len; i++) {
            if (!usb_hid_is_active()) { free(job.seq); job.seq = NULL; break; }
            uint8_t code  = job.seq[i * 2];
            uint8_t shift = job.seq[i * 2 + 1];

            if (shift) {
                int bi = 0xE1 / 8, bp = 0xE1 % 8;
                s_hid_key_state[bi] |= (uint8_t)(1U << bp);
                hid_keyboard_rebuild_and_send();
                vTaskDelay(pdMS_TO_TICKS(5));
            }

            int bi = code / 8, bp = code % 8;
            s_hid_key_state[bi] |= (uint8_t)(1U << bp);
            hid_keyboard_rebuild_and_send();
            vTaskDelay(pdMS_TO_TICKS(12));

            s_hid_key_state[bi] &= (uint8_t)(~(1U << bp));
            hid_keyboard_rebuild_and_send();

            if (shift) {
                vTaskDelay(pdMS_TO_TICKS(5));
                bi = 0xE1 / 8; bp = 0xE1 % 8;
                s_hid_key_state[bi] &= (uint8_t)(~(1U << bp));
                hid_keyboard_rebuild_and_send();
            }

            vTaskDelay(pdMS_TO_TICKS(15));
        }

        free(job.seq);
    }
}

esp_err_t paste_task_init(void)
{
    if (s_paste_queue != NULL) {
        return ESP_OK;
    }

    s_paste_queue = xQueueCreate(2, sizeof(paste_job_t));
    if (!s_paste_queue) {
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(paste_task_func, "paste", 3072, NULL,
                    tskIDLE_PRIORITY + 2, &s_paste_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

/* ========================================== 路由表 ============================================= */

static const web_route_t s_usb_routes[] = {
    {"/api/usb/status", HTTP_GET,  web_module_usb_status_handler, false},
    {"/api/usb/mode",   HTTP_POST, web_module_usb_mode_handler,   false},
    {"/api/hid/ws",     HTTP_GET,  web_module_hid_ws_handler,     true},
};

/* ========================================== 公共 API ============================================= */

void usb_web_handler_init(void)
{
    paste_task_init();
    web_module_register_routes(s_usb_routes,
                               sizeof(s_usb_routes) / sizeof(s_usb_routes[0]));
    ESP_LOGI(TAG, "usb web handlers registered (%d routes)",
             sizeof(s_usb_routes) / sizeof(s_usb_routes[0]));
}

/**
 * @brief 编译期自动注册本模块初始化函数到 Web 模块
 */
static void __attribute__((constructor, used)) usb_web_handler_auto_init(void)
{
    web_module_register_handler("usb", usb_web_handler_init);
}
