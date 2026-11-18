/**
 * @file    usb_hid_receiver.c
 * @brief   USB HID UDP 接收器实现
 *
 * 监听 UDP 8888 端口，接收 PC 端 Python 脚本发来的键鼠事件，
 * 将单键事件转译为 Boot Keyboard 全报告后通过 usb_hid_bridge 发送。
 *
 * 协议格式（小端序）:
 *   广播: [0x00]           → 应答 "HID_ACK"（7 字节）
 *   鼠标: [0x01, dx, dy]   → 相对位移
 *         [0x02, mask]     → 按键位掩码
 *         [0x03, delta]    → 滚轮
 *   键盘: [0x04, 1|0, kc]  → 按下/抬起，kc 为 Python HID_MAP 中的键码
 *
 * @date    2026-05-28
 * @author  Mr.Tang
 */

#include "usb_hid_receiver.h"

#include <string.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"

#include "usb_hid_bridge.h"

static const char *TAG = "[hid_recv]";

#define HID_UDP_PORT            8888        /**< UDP 监听端口 */
#define HID_UDP_RX_BUF_SIZE     32          /**< 接收缓冲区 */
#define HID_UDP_ACK_MSG         "HID_ACK"   /**< 广播发现应答 */
#define HID_MAX_REGULAR_KEYS    6           /**< Boot Keyboard 最大常规键数 */
#define HID_MODIFIER_COUNT      8           /**< 修饰键数量 */
#define HID_STACK_SIZE          3072        /**< 接收任务栈大小 */

/* ========================================== 键盘状态管理 ============================================= */

static bool          s_key_pressed[256];           /**< 按键状态表（按 HID usage 索引） */
static uint8_t       s_regular_keys[HID_MAX_REGULAR_KEYS]; /**< 当前 6 键数组 */
static uint8_t       s_key_count;                 /**< 当前常规键数量 */
static uint8_t       s_modifier_byte;             /**< 当前修饰键位掩码 */
static TaskHandle_t  s_receiver_task = NULL;      /**< 接收任务句柄 */
static int           s_socket = -1;               /**< UDP socket 描述符 */

/**
 * @brief 重新构建并发送 Boot Keyboard Report
 *
 * Python 端已发来标准 HID Usage Code：
 *   - 0xE0-0xE7 → 修饰键，放入 modifier byte
 *   - 其余非零 → 常规键，放入 6 键数组
 *
 * 无需任何转换表，直接透传。
 */
static void hid_rebuild_and_send(void)
{
    s_key_count = 0;
    memset(s_regular_keys, 0, sizeof(s_regular_keys));

    for (uint16_t kc = 0; kc < 256; kc++) {
        if (s_key_pressed[kc] && s_key_count < HID_MAX_REGULAR_KEYS) {
            s_regular_keys[s_key_count++] = (uint8_t)kc;
        }
    }

    usb_hid_bridge_send_keyboard(s_modifier_byte, s_regular_keys);
}

/* ========================================== UDP 接收任务 ============================================= */

/**
 * @brief 安全释放所有按键和鼠标按钮
 *
 * 当超时无数据时调用，强制清空键盘 modifier/6 键数组和鼠标按钮状态。
 */
static void hid_release_all(uint8_t *prev_buttons)
{
    s_modifier_byte = 0;
    memset(s_key_pressed, 0, sizeof(s_key_pressed));
    hid_rebuild_and_send();
    *prev_buttons = 0;
    usb_hid_bridge_send_mouse(0, 0, 0, 0);
    ESP_LOGW(TAG, "Safety timeout: all keys/buttons released");
}

/**
 * @brief UDP HID 数据接收任务
 *
 * 监听 UDP 8888 端口，解析鼠标/键盘数据包。
 * 内建 2 秒安全超时：无数据时自动释放所有按键，
 * 防止 Python 端意外断开后键鼠"卡死"。
 */
static void hid_receiver_task(void *pvParameters)
{
    (void)pvParameters;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket: %d", errno);
        s_receiver_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    s_socket = sock;

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family      = AF_INET;
    local.sin_port        = htons(HID_UDP_PORT);
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0) {
        ESP_LOGE(TAG, "Failed to bind port %d: %d", HID_UDP_PORT, errno);
        close(sock);
        s_socket = -1;
        s_receiver_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    /* 设置 500ms recv 超时以支持安全超时检查 */
    struct timeval tv = { .tv_sec = 0, .tv_usec = 500000 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    ESP_LOGI(TAG, "Listening on UDP port %d", HID_UDP_PORT);

    uint8_t             rx_buf[HID_UDP_RX_BUF_SIZE];
    struct sockaddr_in  remote;
    socklen_t           remote_len;
    uint8_t             prev_buttons = 0;

    TickType_t          last_activity = xTaskGetTickCount();
    const TickType_t    safety_timeout = pdMS_TO_TICKS(2000);
    bool                s_is_locked = false;

    while (1) {
        remote_len = sizeof(remote);
        int len = recvfrom(sock, rx_buf, sizeof(rx_buf), 0,
                           (struct sockaddr *)&remote, &remote_len);

        if (len <= 0) {
            if (errno == EBADF) {
                // socket 已被 close，退出任务
                break;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                if (s_is_locked) {
                    TickType_t now = xTaskGetTickCount();
                    if ((now - last_activity) > safety_timeout) {
                        hid_release_all(&prev_buttons);
                        s_is_locked = false;
                    }
                }
            } else {
                ESP_LOGW(TAG, "recvfrom error: %d", errno);
            }
            continue;
        }

        last_activity = xTaskGetTickCount();

        switch (rx_buf[0]) {
            /* --- 广播发现 --- */
            case 0x00:
                sendto(sock, HID_UDP_ACK_MSG, (int)strlen(HID_UDP_ACK_MSG), 0,
                       (struct sockaddr *)&remote, sizeof(remote));
                break;

            /* --- 鼠标移动 --- */
            case 0x01:
                if (len >= 3) {
                    usb_hid_bridge_send_mouse(prev_buttons,
                                              (int8_t)rx_buf[1],
                                              (int8_t)rx_buf[2], 0);
                }
                break;

            /* --- 鼠标按键 --- */
            case 0x02:
                if (len >= 2) {
                    prev_buttons = rx_buf[1];
                    usb_hid_bridge_send_mouse(prev_buttons, 0, 0, 0);
                    if (prev_buttons != 0) {
                        s_is_locked = true;
                    }
                }
                break;

            /* --- 鼠标滚轮 --- */
            case 0x03:
                if (len >= 2) {
                    usb_hid_bridge_send_mouse(prev_buttons, 0, 0, (int8_t)rx_buf[1]);
                }
                break;

            /* --- 键盘按下/抬起 ---
             *  HID Usage: 0xE0-0xE7 → modifier byte，其余 → 6 键数组
             */
            case 0x04:
                if (len >= 3) {
                    uint8_t pressed = rx_buf[1];
                    uint8_t hid_kc  = rx_buf[2];

                    if (hid_kc >= 0xE0 && hid_kc <= 0xE7) {
                        uint8_t mod_bit = (uint8_t)(1U << (hid_kc - 0xE0));
                        if (pressed) {
                            s_modifier_byte |= mod_bit;
                            s_is_locked = true;
                        } else {
                            s_modifier_byte &= (uint8_t)(~mod_bit);
                        }
                    } else if (hid_kc != 0) {
                        s_key_pressed[hid_kc] = (pressed != 0);
                        if (pressed) {
                            s_is_locked = true;
                        }
                    }
                    hid_rebuild_and_send();
                }
                break;

            default:
                break;
        }
    }

    // 任务退出清理（socket 已被 receiver_stop 关闭）
    s_socket = -1;
    s_receiver_task = NULL;
    ESP_LOGI(TAG, "Receiver task exiting");
    vTaskDelete(NULL);
}

/* ========================================== 公共 API ============================================= */

/**
 * @brief 参见 usb_hid_receiver.h
 */
esp_err_t usb_hid_receiver_start(void)
{
    if (s_receiver_task != NULL) {
        ESP_LOGW(TAG, "Receiver already running");
        return ESP_OK;
    }

    if (xTaskCreate(hid_receiver_task, "hid_recv", HID_STACK_SIZE,
                    NULL, tskIDLE_PRIORITY + 4, &s_receiver_task) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create receiver task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "HID receiver started");
    return ESP_OK;
}

/**
 * @brief 参见 usb_hid_receiver.h
 */
void usb_hid_receiver_stop(void)
{
    if (s_receiver_task == NULL) {
        return;
    }

    // 关闭 socket 触发 recvfrom 返回 EBADF，任务自退出避免 vTaskDelete 死锁
    if (s_socket >= 0) {
        close(s_socket);
        s_socket = -1;
    }

    // 等待任务自然退出（最多 600ms，覆盖一次 recvfrom 超时）
    int timeout = 60;
    while (s_receiver_task != NULL && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // 极端情况：任务未退出则强制删除
    if (s_receiver_task != NULL) {
        ESP_LOGW(TAG, "Receiver task did not exit, force deleting");
        vTaskDelete(s_receiver_task);
        s_receiver_task = NULL;
    }

    ESP_LOGI(TAG, "HID receiver stopped");
}
