/**
 * @file    storage_web_handler.c
 * @brief   SD 卡文件管理 Web handler 实现（MSC 存储相关）
 *
 * 提供 SD 卡文件管理 REST API（列表/上传/下载/预览/删除/重命名/移动/新建目录/状态查询）。
 *
 * @date    2026-07-26
 * @author  Mr.Tang
 */

#include "web_module.h"

#include "storage_module.h"
#include "usb_manager.h"

#include "event_bus.h"
#include "event_defs.h"

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"

#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

static const char *TAG = "[storage_web]";

/* ========================================== 配置宏 ============================================= */

/** @brief 单次下载/预览读取块大小（8KB，减少内部 RAM 碎片压力） */
#define DOWNLOAD_CHUNK_SIZE  (8 * 1024)

/** @brief 预览 Range 单次响应上限（1MB，平衡缓冲时长与 seek 响应速度） */
#define PREVIEW_RANGE_MAX    (1 * 1024 * 1024)

/* ========================================== 辅助函数（本地重新实现，消除对 web_module.c 内部 static 函数的依赖） ============================================= */

/**
 * @brief 十六进制字符转数值
 *
 * @param c 十六进制字符（0-9, a-f, A-F）
 * @return 对应数值（0-15），无效返回 -1
 */
static int storage_hex_to_int(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    c = (char)tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    return -1;
}

/**
 * @brief URL 编码字符串原地解码
 *
 * 处理 "%" + 2 位十六进制编码以及 "+" → 空格。
 *
 * @param str 待解码字符串（原地修改）
 */
static void storage_url_decode_inplace(char *str)
{
    if (str == NULL) {
        return;
    }

    char *src = str;
    char *dst = str;

    while (*src != '\0') {
        if (*src == '%' && isxdigit((unsigned char)src[1])
            && isxdigit((unsigned char)src[2])) {
            int hi = storage_hex_to_int(src[1]);
            int lo = storage_hex_to_int(src[2]);
            if (hi >= 0 && lo >= 0) {
                *dst = (char)((hi << 4) | lo);
                src += 3;
            } else {
                *dst = *src;
                src++;
            }
        } else if (*src == '+') {
            *dst = ' ';
            src++;
        } else {
            *dst = *src;
            src++;
        }
        dst++;
    }

    *dst = '\0';
}

/**
 * @brief 检查路径中是否包含路径遍历攻击（如 ../）
 *
 * @param path 待检查路径字符串
 * @return true=包含路径遍历，false=安全
 */
static bool is_path_traversal(const char *path)
{
    if (!path) {
        return false;
    }

    if (strcmp(path, "..") == 0) {
        return true;
    }

    if (strstr(path, "/../") != NULL) {
        return true;
    }
    if (strstr(path, "\\..\\") != NULL) {
        return true;
    }

    size_t len = strlen(path);
    if (len >= 3) {
        if (strncmp(path, "../", 3) == 0) return true;
        if (strncmp(path, "..\\", 3) == 0) return true;
        if (strcmp(path + len - 3, "/..") == 0) return true;
        if (strcmp(path + len - 3, "\\..") == 0) return true;
    }

    return false;
}

/**
 * @brief 设置文件/目录的时间戳
 *
 * @param path      文件路径
 * @param timestamp 目标时间戳（Unix 时间）
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
static esp_err_t storage_set_file_timestamp(const char *path, time_t timestamp)
{
    struct utimbuf ut;
    ut.actime = timestamp;
    ut.modtime = timestamp;
    return (utime(path, &ut) == 0) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief 从 HTTP 请求中提取文件名参数
 *
 * 优先解析查询字符串中的 "filename" 键，若无则回退到整个查询字符串。
 *
 * @param req           HTTP 请求对象
 * @param filename      输出缓冲区
 * @param filename_size 输出缓冲区大小
 * @return ESP_OK 成功，其他失败
 */
static esp_err_t storage_extract_filename(httpd_req_t *req, char *filename,
                                          size_t filename_size)
{
    char query[256] = {0};
    esp_err_t ret = httpd_req_get_url_query_str(req, query, sizeof(query));
    if (ret != ESP_OK) {
        return ret;
    }

    ret = httpd_query_key_value(query, "filename", filename, filename_size);
    if (ret == ESP_OK && filename[0] != '\0') {
        storage_url_decode_inplace(filename);
        return ESP_OK;
    }

    if (query[0] != '\0') {
        strncpy(filename, query, filename_size - 1);
        filename[filename_size - 1] = '\0';
        storage_url_decode_inplace(filename);
        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief 根据文件扩展名返回对应的 Content-Type
 *
 * @param filename 文件名或路径
 * @return MIME 类型字符串
 */
static const char *storage_get_content_type(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (ext == NULL) {
        return "text/plain";
    }
    if (strcmp(ext, ".htm") == 0 || strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".ico") == 0) return "image/x-icon";
    if (strcmp(ext, ".xml") == 0) return "text/xml";
    if (strcmp(ext, ".pdf") == 0) return "application/x-pdf";
    if (strcmp(ext, ".zip") == 0) return "application/x-zip";
    if (strcmp(ext, ".gz") == 0) return "application/x-gzip";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcmp(ext, ".mp4") == 0) return "video/mp4";
    if (strcmp(ext, ".webm") == 0) return "video/webm";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".flac") == 0) return "audio/flac";
    if (strcmp(ext, ".ogg") == 0) return "audio/ogg";
    if (strcmp(ext, ".woff") == 0) return "font/woff";
    if (strcmp(ext, ".woff2") == 0) return "font/woff2";
    if (strcmp(ext, ".ttf") == 0) return "font/ttf";
    if (strcmp(ext, ".eot") == 0) return "application/vnd.ms-fontobject";
    return "text/plain";
}

/**
 * @brief 递归删除文件或目录
 *
 * @param path 文件或目录路径
 * @return 0 成功，-1 失败
 */
static int remove_recursive(const char *path)
{
    struct stat statbuf;

    if (stat(path, &statbuf) != 0) {
        return -1;
    }

    if (S_ISDIR(statbuf.st_mode)) {
        DIR *d = opendir(path);
        if (d == NULL) {
            return -1;
        }

        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0
                || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char *child_path = (char *)heap_caps_malloc(1024, MALLOC_CAP_SPIRAM);
            if (child_path == NULL) {
                closedir(d);
                return -1;
            }
            snprintf(child_path, 1024, "%s/%s", path, entry->d_name);

            if (remove_recursive(child_path) != 0) {
                free(child_path);
                closedir(d);
                return -1;
            }
            free(child_path);
        }

        closedir(d);
        return rmdir(path);
    } else {
        return remove(path);
    }
}

/* ========================================== 异步写入流水线（乒乓缓冲解耦 HTTP 接收与 SD 卡写入） ============================================= */

#define ASYNC_BUF_COUNT 2
#define ASYNC_BUF_SIZE  (16 * 1024)

/** @brief 异步写入任务作业 */
typedef struct {
    char              *buf;      /**< 数据缓冲区指针 */
    size_t             size;     /**< 有效数据长度 */
    int                fd;       /**< 目标文件描述符 */
    bool               is_final; /**< 是否为最后一个 chunk */
    EventGroupHandle_t done_evt; /**< 完成通知事件组 */
} write_job_t;

static QueueHandle_t      s_write_queue = NULL; /**< 异步写入作业队列 */
static TaskHandle_t       s_write_task  = NULL; /**< 异步写入任务句柄 */
static char              *s_async_bufs[ASYNC_BUF_COUNT]; /**< 乒乓缓冲数组 */
static SemaphoreHandle_t  s_buf_free[ASYNC_BUF_COUNT];   /**< 缓冲可用信号量 */

/**
 * @brief 异步文件写入任务
 *
 * 从队列取出 write_job_t，执行 write()；若为最后一个 chunk 则额外执行 fsync()+close()
 * 并通过 done_evt 通知调用方。
 */
static void file_write_task(void *arg)
{
    (void)arg;
    write_job_t job;

    while (1) {
        if (xQueueReceive(s_write_queue, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        ssize_t written = write(job.fd, job.buf, job.size);
        if (written != (ssize_t)job.size) {
            ESP_LOGE(TAG, "[async_write] write failed: %d/%u bytes",
                     (int)written, (unsigned)job.size);
        }

        int buf_idx = (job.buf == s_async_bufs[0]) ? 0 : 1;
        xSemaphoreGive(s_buf_free[buf_idx]);

        if (job.is_final) {
            fsync(job.fd);
            close(job.fd);
            if (job.done_evt) {
                xEventGroupSetBits(job.done_evt, 1);
            }
        }
    }
}

/**
 * @brief 初始化异步写入流水线
 *
 * 分配 DMA 缓冲、创建写入任务和作业队列。
 */
static esp_err_t async_write_init(void)
{
    if (s_write_queue != NULL) {
        return ESP_OK;
    }

    s_write_queue = xQueueCreate(1, sizeof(write_job_t));
    if (!s_write_queue) {
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < ASYNC_BUF_COUNT; i++) {
        s_async_bufs[i] = heap_caps_malloc(ASYNC_BUF_SIZE, MALLOC_CAP_DMA);
        if (!s_async_bufs[i]) {
            ESP_LOGE(TAG, "[async_write] DMA buf[%d] alloc failed", i);
            return ESP_ERR_NO_MEM;
        }
    }

    for (int i = 0; i < ASYNC_BUF_COUNT; i++) {
        s_buf_free[i] = xSemaphoreCreateBinary();
        if (!s_buf_free[i]) {
            return ESP_ERR_NO_MEM;
        }
        xSemaphoreGive(s_buf_free[i]);
    }

    if (xTaskCreate(file_write_task, "file_wr", 4096, NULL,
                    tskIDLE_PRIORITY + 3, &s_write_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "[async_write] pipeline ready: %u×%uKB DMA bufs",
             ASYNC_BUF_COUNT, ASYNC_BUF_SIZE / 1024);
    return ESP_OK;
}

/**
 * @brief 发送一个缓冲到写入任务（阻塞直到写入任务可取走）
 */
static void async_write_send(char *buf, size_t size, int fd, bool is_final,
                             EventGroupHandle_t done_evt)
{
    write_job_t job = {
        .buf = buf, .size = size, .fd = fd,
        .is_final = is_final, .done_evt = done_evt
    };
    xQueueSend(s_write_queue, &job, portMAX_DELAY);
}

/**
 * @brief 等待最后一个 chunk 的写入完成（阻塞）
 */
static void async_write_wait_done(EventGroupHandle_t evt)
{
    xEventGroupWaitBits(evt, 1, pdTRUE, pdTRUE, portMAX_DELAY);
    vEventGroupDelete(evt);
}

/* ========================================== Handler 实现 ============================================= */

/* -------------------- 文件列表 -------------------- */

/**
 * @brief GET /api/msc/filelist
 *
 * 返回 SD 卡指定目录下的文件/目录列表（JSON 格式，分块传输）。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t storage_msc_filelist_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    char dir_path[512] = "/disk";
    char query[256] = {0};
    esp_err_t ret = httpd_req_get_url_query_str(req, query, sizeof(query));
    if (ret == ESP_OK) {
        char dir_param[256] = {0};
        ret = httpd_query_key_value(query, "dir", dir_param, sizeof(dir_param));
        if (ret == ESP_OK && dir_param[0] != '\0') {
            storage_url_decode_inplace(dir_param);
            snprintf(dir_path, sizeof(dir_path), "/disk/%s", dir_param);
            if (is_path_traversal(dir_path)) {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid directory path");
                return ESP_OK;
            }
        }
    }

    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        httpd_resp_send(req, "{\"files\":null,\"error\":\"not_mounted\"}", 39);
        return ESP_OK;
    }

    char   buf[1024];
    size_t offset = 0;

    offset = snprintf(buf, sizeof(buf), "{\"files\":[");
    if (httpd_resp_send_chunk(req, buf, offset) != ESP_OK) {
        closedir(dir);
        return ESP_OK;
    }

    int          first = 1;
    struct dirent *entry;
    char         filepath[1024];
    struct stat  stat_buf;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        if (!first) {
            if (httpd_resp_send_chunk(req, ",", 1) != ESP_OK) {
                closedir(dir);
                httpd_resp_send_chunk(req, NULL, 0);
                return ESP_OK;
            }
        }
        first = 0;

        snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, entry->d_name);
        int    file_size = 0;
        time_t mod_time  = 0;
        if (stat(filepath, &stat_buf) == 0) {
            file_size = stat_buf.st_size;
            mod_time  = stat_buf.st_mtime;
        }

        if (entry->d_type == DT_DIR) {
            offset = snprintf(buf, sizeof(buf),
                "{\"name\":\"%s\",\"type\":\"directory\",\"size\":0,\"mtime\":%lld}",
                entry->d_name, (long long)mod_time);
        } else {
            offset = snprintf(buf, sizeof(buf),
                "{\"name\":\"%s\",\"type\":\"file\",\"size\":%d,\"mtime\":%lld}",
                entry->d_name, file_size, (long long)mod_time);
        }

        if (httpd_resp_send_chunk(req, buf, offset) != ESP_OK) {
            closedir(dir);
            httpd_resp_send_chunk(req, NULL, 0);
            return ESP_OK;
        }
    }

    closedir(dir);

    if (httpd_resp_send_chunk(req, "]}", 2) != ESP_OK) {
        httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
    }

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* -------------------- 文件上传（异步乒乓缓冲，解耦 HTTP 接收与 SD 卡写入） -------------------- */

/**
 * @brief POST /api/msc/upload
 *
 * 使用异步乒乓缓冲流水线接收 HTTP body 并写入 SD 卡，
 * 双层缓冲区使 HTTP recv 与 SD write 可以并行执行，提升上传速度。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t storage_msc_upload_handler(httpd_req_t *req)
{
    char   filename[256] = {0};
    char   filepath[1024];
    time_t mtime = 0;

    if (storage_extract_filename(req, filename, sizeof(filename)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing filename");
        return ESP_OK;
    }

    char query[256] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char mtime_str[64] = {0};
        if (httpd_query_key_value(query, "mtime", mtime_str, sizeof(mtime_str)) == ESP_OK) {
            mtime = (time_t)atoll(mtime_str);
        }
    }

    snprintf(filepath, sizeof(filepath), "/disk/%s", filename);

    if (is_path_traversal(filepath)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
        return ESP_OK;
    }

    ESP_LOGD(TAG, "Uploading file: %s to %s", filename, filepath);

    struct stat stat_buf;
    if (stat(filepath, &stat_buf) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        return ESP_OK;
    }

    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
        return ESP_OK;
    }

    event_bus_post(EVENT_FILE_UPLOAD_STARTED, NULL, 0);

    char               *cur      = s_async_bufs[0];
    size_t              buf_used = 0;
    int                 remaining = req->content_len;
    EventGroupHandle_t  done_evt = xEventGroupCreate();

    xSemaphoreTake(s_buf_free[0], portMAX_DELAY);

    while (remaining > 0) {
        int space   = ASYNC_BUF_SIZE - (int)buf_used;
        int to_read = (remaining < space) ? remaining : space;
        int recv_ret = httpd_req_recv(req, cur + buf_used, to_read);

        if (recv_ret <= 0) {
            if (recv_ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            async_write_send(cur, buf_used, fd, true, done_evt);
            async_write_wait_done(done_evt);
            unlink(filepath);
            event_bus_post(EVENT_FILE_UPLOAD_FAILED, NULL, 0);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Failed to receive data");
            return ESP_OK;
        }

        buf_used += (size_t)recv_ret;
        remaining -= recv_ret;

        if (buf_used >= ASYNC_BUF_SIZE || remaining <= 0) {
            bool is_last = (remaining <= 0);
            async_write_send(cur, buf_used, fd, is_last,
                             is_last ? done_evt : NULL);
            cur = (cur == s_async_bufs[0]) ? s_async_bufs[1] : s_async_bufs[0];
            buf_used = 0;
            if (!is_last) {
                int next_idx = (cur == s_async_bufs[0]) ? 0 : 1;
                xSemaphoreTake(s_buf_free[next_idx], portMAX_DELAY);
            }
        }
    }

    async_write_wait_done(done_evt);

    event_bus_post(EVENT_FILE_UPLOAD_FINISHED, NULL, 0);

    if (mtime > 0) {
        storage_set_file_timestamp(filepath, mtime);
    }

    ESP_LOGD(TAG, "File uploaded successfully: %s", filename);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", strlen("{\"ok\":true}"));

    return ESP_OK;
}

/* -------------------- 文件下载 -------------------- */

/**
 * @brief GET /api/msc/download
 *
 * 原始文件下载（无 Range 支持），8KB DMA 缓冲分块发送。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t storage_msc_download_handler(httpd_req_t *req)
{
    char filename[256] = {0};
    char filepath[1024];

    if (storage_extract_filename(req, filename, sizeof(filename)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing filename");
        return ESP_OK;
    }

    snprintf(filepath, sizeof(filepath), "/disk/%s", filename);

    if (is_path_traversal(filepath)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
        return ESP_OK;
    }

    ESP_LOGD(TAG, "Downloading file: %s from %s", filename, filepath);

    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_OK;
    }

    struct stat file_stat;
    if (fstat(fd, &file_stat) != 0) {
        close(fd);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get file size");
        return ESP_OK;
    }
    off_t file_size = file_stat.st_size;

    event_bus_post(EVENT_FILE_DOWNLOAD_STARTED, NULL, 0);

    httpd_resp_set_type(req, "application/octet-stream");

    char content_length[20];
    snprintf(content_length, sizeof(content_length), "%lld", (long long)file_size);
    httpd_resp_set_hdr(req, "Content-Length", content_length);

    const char *pure_filename = strrchr(filename, '/');
    if (pure_filename) {
        pure_filename++;
    } else {
        pure_filename = filename;
    }
    char content_disposition[512];
    snprintf(content_disposition, sizeof(content_disposition),
             "attachment; filename=\"%s\"", pure_filename);
    httpd_resp_set_hdr(req, "Content-Disposition", content_disposition);

    char *buf = heap_caps_malloc(DOWNLOAD_CHUNK_SIZE, MALLOC_CAP_DMA);
    if (buf == NULL) {
        close(fd);
        event_bus_post(EVENT_FILE_DOWNLOAD_FAILED, NULL, 0);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to allocate buffer");
        return ESP_OK;
    }

    long    remaining = file_size;
    ssize_t read_ret;

    while (remaining > 0) {
        int to_read = (remaining < DOWNLOAD_CHUNK_SIZE) ? (int)remaining : DOWNLOAD_CHUNK_SIZE;
        read_ret = read(fd, buf, to_read);
        if (read_ret <= 0) {
            break;
        }
        if (httpd_resp_send_chunk(req, buf, read_ret) != ESP_OK) {
            close(fd);
            heap_caps_free(buf);
            event_bus_post(EVENT_FILE_DOWNLOAD_FAILED, NULL, 0);
            return ESP_OK;
        }
        remaining -= read_ret;
    }
    heap_caps_free(buf);

    close(fd);
    httpd_resp_send_chunk(req, NULL, 0);
    event_bus_post(EVENT_FILE_DOWNLOAD_FINISHED, NULL, 0);
    ESP_LOGD(TAG, "File downloaded successfully: %s", filename);
    return ESP_OK;
}

/* -------------------- 文件预览（Range 支持） -------------------- */

/**
 * @brief GET /api/msc/preview
 *
 * 支持 HTTP Range 请求的流媒体播放，用于视频/音频渐进加载。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t storage_msc_preview_handler(httpd_req_t *req)
{
    char filename[256] = {0};
    char filepath[1024];

    if (storage_extract_filename(req, filename, sizeof(filename)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing filename");
        return ESP_OK;
    }

    snprintf(filepath, sizeof(filepath), "/disk/%s", filename);

    if (is_path_traversal(filepath)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
        return ESP_OK;
    }

    ESP_LOGD(TAG, "Previewing file: %s from %s", filename, filepath);

    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_OK;
    }

    struct stat file_stat;
    if (fstat(fd, &file_stat) != 0) {
        close(fd);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get file size");
        return ESP_OK;
    }
    off_t file_size = file_stat.st_size;

    long range_start = 0;
    long range_end   = file_size - 1;
    int  has_range   = 0;

    httpd_resp_set_hdr(req, "Accept-Ranges", "bytes");

    char range_str[128] = {0};
    if (httpd_req_get_hdr_value_str(req, "Range", range_str, sizeof(range_str)) == ESP_OK) {
        int parsed = sscanf(range_str, "bytes=%ld-%ld", &range_start, &range_end);
        if (parsed >= 1) {
            if (parsed == 1) {
                range_end = range_start + PREVIEW_RANGE_MAX - 1;
            }
            if (range_end >= file_size) {
                range_end = file_size - 1;
            }
            if (range_start >= 0 && range_start < file_size && range_start <= range_end) {
                has_range = 1;

                httpd_resp_set_status(req, "206 Partial Content");
                httpd_resp_set_type(req, storage_get_content_type(filename));

                char content_range[64];
                snprintf(content_range, sizeof(content_range), "bytes %ld-%ld/%lld",
                         range_start, range_end, (long long)file_size);
                httpd_resp_set_hdr(req, "Content-Range", content_range);
            }
        }
    }

    long total_to_send = has_range ? (range_end - range_start + 1) : file_size;

    char content_length[20];
    snprintf(content_length, sizeof(content_length), "%ld", total_to_send);
    httpd_resp_set_hdr(req, "Content-Length", content_length);

    if (!has_range) {
        httpd_resp_set_type(req, storage_get_content_type(filename));
    }

    lseek(fd, has_range ? range_start : 0, SEEK_SET);

    char *chunk = heap_caps_malloc(DOWNLOAD_CHUNK_SIZE, MALLOC_CAP_DMA);
    if (chunk == NULL) {
        close(fd);
        event_bus_post(EVENT_FILE_DOWNLOAD_FAILED, NULL, 0);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to allocate buffer");
        return ESP_OK;
    }

    long    remaining = total_to_send;
    ssize_t read_ret;

    while (remaining > 0) {
        int to_read = (remaining < DOWNLOAD_CHUNK_SIZE) ? (int)remaining : DOWNLOAD_CHUNK_SIZE;
        read_ret = read(fd, chunk, to_read);
        if (read_ret <= 0) {
            event_bus_post(EVENT_FILE_DOWNLOAD_FAILED, NULL, 0);
            break;
        }
        if (httpd_resp_send_chunk(req, chunk, read_ret) != ESP_OK) {
            close(fd);
            heap_caps_free(chunk);
            event_bus_post(EVENT_FILE_DOWNLOAD_FAILED, NULL, 0);
            return ESP_OK;
        }
        remaining -= read_ret;
    }
    heap_caps_free(chunk);

    close(fd);
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

/* -------------------- 批量删除 -------------------- */

/**
 * @brief POST /api/msc/batch-delete
 *
 * 请求格式 JSON: {"files":["file1.txt","dir2",...],"dir":"sub/path"}
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t storage_msc_batch_delete_handler(httpd_req_t *req)
{
    char *buf = (char *)heap_caps_malloc(1024, MALLOC_CAP_SPIRAM);
    if (buf == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_OK;
    }
    memset(buf, 0, 1024);
    int ret = httpd_req_recv(req, buf, 1023);

    if (ret <= 0) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad request");
        return ESP_OK;
    }

    buf[ret] = '\0';

    ESP_LOGD(TAG, "Batch delete JSON: %s", buf);

    char dir[256] = {0};

    for (int i = 0; buf[i] != '\0'; i++) {
        if (strncmp(&buf[i], "\"dir\":", 6) == 0) {
            i += 6;
            while (buf[i] == ' ') i++;
            if (buf[i] == '\"') i++;
            int j = 0;
            while (buf[i] != '\"' && buf[i] != '\0' && j < sizeof(dir) - 1) {
                dir[j++] = buf[i++];
            }
            dir[j] = '\0';
            ESP_LOGD(TAG, "Current directory: %s", dir);
            break;
        }
    }

    bool in_files       = false;
    bool in_array       = false;
    bool in_file_string = false;
    char current_file[256] = {0};
    int  current_file_len  = 0;

    for (int i = 0; buf[i] != '\0'; i++) {
        if (strncmp(&buf[i], "\"files\":", 7) == 0) {
            in_files = true;
            i += 7;
            continue;
        }

        if (in_files && !in_array && buf[i] == '[') {
            in_array = true;
            continue;
        }

        if (in_files && in_array && buf[i] == ']') {
            break;
        }

        if (in_files && in_array && buf[i] == '\"' && !in_file_string) {
            in_file_string = true;
            continue;
        }

        if (in_files && in_array && buf[i] == '\"' && in_file_string) {
            in_file_string = false;

            if (current_file_len == 0) {
                continue;
            }

            char filepath[1024];
            if (strlen(dir) > 0) {
                snprintf(filepath, sizeof(filepath), "/disk/%s/%s", dir, current_file);
            } else {
                snprintf(filepath, sizeof(filepath), "/disk/%s", current_file);
            }

            ESP_LOGD(TAG, "Deleting: %s", filepath);

            if (is_path_traversal(filepath)) {
                memset(current_file, 0, sizeof(current_file));
                current_file_len = 0;
                continue;
            }

            if (remove_recursive(filepath) == 0) {
                ESP_LOGD(TAG, "Deleted: %s", filepath);
            } else {
                ESP_LOGW(TAG, "Delete failed: %s", filepath);
            }

            memset(current_file, 0, sizeof(current_file));
            current_file_len = 0;
            continue;
        }

        if (in_files && in_array && in_file_string) {
            if (current_file_len < sizeof(current_file) - 1) {
                current_file[current_file_len++] = buf[i];
            }
            continue;
        }
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", strlen("{\"ok\":true}"));
    free(buf);
    return ESP_OK;
}

/* -------------------- 软重启 MSC -------------------- */

/**
 * @brief POST /api/msc/restart
 *
 * 调用 usb_manager_restart() 重新枚举 USB MSC 设备。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t storage_msc_restart_handler(httpd_req_t *req)
{
    if (usb_manager_restart() == ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":true}", strlen("{\"ok\":true}"));
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to restart USB");
    }
    return ESP_OK;
}

/* -------------------- 新建文件夹 -------------------- */

/**
 * @brief POST /api/msc/mkdir
 *
 * 请求格式 JSON: {"dir":"parent","name":"newfolder","mtime":1234567890}
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t storage_msc_mkdir_handler(httpd_req_t *req)
{
    char buf[512] = {0};
    int  ret = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad request");
        return ESP_OK;
    }

    buf[ret] = '\0';

    ESP_LOGD(TAG, "Create dir JSON: %s", buf);

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    cJSON *dir_obj   = cJSON_GetObjectItem(root, "dir");
    cJSON *name_obj  = cJSON_GetObjectItem(root, "name");
    cJSON *mtime_obj = cJSON_GetObjectItem(root, "mtime");

    if (!name_obj || !cJSON_IsString(name_obj) || name_obj->valuestring[0] == '\0') {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing folder name");
        return ESP_OK;
    }

    const char *dir   = (dir_obj && cJSON_IsString(dir_obj)) ? dir_obj->valuestring : "";
    const char *name  = name_obj->valuestring;
    time_t      mtime = (mtime_obj && cJSON_IsNumber(mtime_obj))
                        ? (time_t)mtime_obj->valuedouble : 0;

    char full_path[PATH_MAX];
    if (dir[0] != '\0') {
        snprintf(full_path, sizeof(full_path), "/disk/%s/%s", dir, name);
    } else {
        snprintf(full_path, sizeof(full_path), "/disk/%s", name);
    }

    cJSON_Delete(root);

    ESP_LOGI(TAG, "Creating directory: %s", full_path);

    if (is_path_traversal(full_path)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path");
        return ESP_OK;
    }

    struct stat stat_buf;
    if (stat(full_path, &stat_buf) == 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req,
            "{\"ok\":false,\"message\":\"Directory already exists\"}",
            strlen("{\"ok\":false,\"message\":\"Directory already exists\"}"));
        return ESP_OK;
    }

    if (mkdir(full_path, 0755) == 0) {
        if (mtime > 0) {
            storage_set_file_timestamp(full_path, mtime);
        }
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":true}", strlen("{\"ok\":true}"));
    } else {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req,
            "{\"ok\":false,\"message\":\"Failed to create directory\"}",
            strlen("{\"ok\":false,\"message\":\"Failed to create directory\"}"));
    }

    return ESP_OK;
}

/* -------------------- 重命名 -------------------- */

/**
 * @brief POST /api/msc/rename
 *
 * 请求格式 JSON: {"dir":"parent","oldName":"a.txt","newName":"b.txt","mtime":123}
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t storage_msc_rename_handler(httpd_req_t *req)
{
    char buf[512] = {0};
    int  ret = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad request");
        return ESP_OK;
    }

    buf[ret] = '\0';

    ESP_LOGD(TAG, "Rename JSON: %s", buf);

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    cJSON *dir_obj     = cJSON_GetObjectItem(root, "dir");
    cJSON *oldName_obj = cJSON_GetObjectItem(root, "oldName");
    cJSON *newName_obj = cJSON_GetObjectItem(root, "newName");
    cJSON *mtime_obj   = cJSON_GetObjectItem(root, "mtime");

    if (!oldName_obj || !cJSON_IsString(oldName_obj)
        || !newName_obj || !cJSON_IsString(newName_obj)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing parameters");
        return ESP_OK;
    }

    const char *dir     = (dir_obj && cJSON_IsString(dir_obj)) ? dir_obj->valuestring : "";
    const char *oldName = oldName_obj->valuestring;
    const char *newName = newName_obj->valuestring;
    time_t      mtime   = (mtime_obj && cJSON_IsNumber(mtime_obj))
                          ? (time_t)mtime_obj->valuedouble : 0;

    char old_path[PATH_MAX];
    char new_path[PATH_MAX];

    if (dir[0] != '\0') {
        snprintf(old_path, sizeof(old_path), "/disk/%s/%s", dir, oldName);
        snprintf(new_path, sizeof(new_path), "/disk/%s/%s", dir, newName);
    } else {
        snprintf(old_path, sizeof(old_path), "/disk/%s", oldName);
        snprintf(new_path, sizeof(new_path), "/disk/%s", newName);
    }

    cJSON_Delete(root);

    ESP_LOGI(TAG, "Renaming: %s -> %s", old_path, new_path);

    if (is_path_traversal(old_path) || is_path_traversal(new_path)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path");
        return ESP_OK;
    }

    if (rename(old_path, new_path) == 0) {
        if (mtime > 0) {
            storage_set_file_timestamp(new_path, mtime);
        }
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"ok\":true}", strlen("{\"ok\":true}"));
    } else {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req,
            "{\"ok\":false,\"message\":\"Failed to rename\"}",
            strlen("{\"ok\":false,\"message\":\"Failed to rename\"}"));
    }

    return ESP_OK;
}

/* -------------------- 移动文件 -------------------- */

/**
 * @brief POST /api/msc/move
 *
 * 请求格式 JSON: {"files":["a.txt"],"sourceDir":"src","targetDir":"dst"}
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t storage_msc_move_handler(httpd_req_t *req)
{
    char buf[512] = {0};
    int  ret = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad request");
        return ESP_OK;
    }

    buf[ret] = '\0';
    ESP_LOGD(TAG, "Move JSON: %s", buf);

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    cJSON *files_obj      = cJSON_GetObjectItem(root, "files");
    cJSON *sourceDir_obj  = cJSON_GetObjectItem(root, "sourceDir");
    cJSON *targetDir_obj  = cJSON_GetObjectItem(root, "targetDir");

    if (!cJSON_IsArray(files_obj) || cJSON_GetArraySize(files_obj) == 0) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing files array");
        return ESP_OK;
    }

    const char *sourceDir = (sourceDir_obj && cJSON_IsString(sourceDir_obj))
                            ? sourceDir_obj->valuestring : "";
    const char *targetDir = (targetDir_obj && cJSON_IsString(targetDir_obj))
                            ? targetDir_obj->valuestring : "";

    if (strcmp(sourceDir, targetDir) == 0) {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req,
            "{\"ok\":false,\"message\":\"Source and target are the same\"}",
            strlen("{\"ok\":false,\"message\":\"Source and target are the same\"}"));
        return ESP_OK;
    }

    int    success_count = 0;
    cJSON *file_item;
    cJSON_ArrayForEach(file_item, files_obj) {
        if (!cJSON_IsString(file_item)) {
            continue;
        }
        const char *filename = file_item->valuestring;

        char old_path[PATH_MAX];
        char new_path[PATH_MAX];

        if (sourceDir[0] != '\0') {
            snprintf(old_path, sizeof(old_path), "/disk/%s/%s", sourceDir, filename);
        } else {
            snprintf(old_path, sizeof(old_path), "/disk/%s", filename);
        }
        if (targetDir[0] != '\0') {
            snprintf(new_path, sizeof(new_path), "/disk/%s/%s", targetDir, filename);
        } else {
            snprintf(new_path, sizeof(new_path), "/disk/%s", filename);
        }

        if (is_path_traversal(old_path) || is_path_traversal(new_path)) {
            continue;
        }

        ESP_LOGD(TAG, "Moving: %s -> %s", old_path, new_path);

        if (rename(old_path, new_path) == 0) {
            success_count++;
        }
    }

    cJSON_Delete(root);

    char response[64];
    snprintf(response, sizeof(response), "{\"ok\":true,\"moved\":%d}", success_count);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

/* -------------------- 存储状态查询 -------------------- */

/**
 * @brief GET /api/msc/status
 *
 * 返回 SD 卡挂载状态及总容量/剩余容量（MB）。
 *
 * @param req HTTP 请求对象
 * @return ESP_OK
 */
static esp_err_t storage_msc_status_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    ESP_LOGI(TAG, "MSC status API called");

    struct stat stat_buf;
    if (stat("/disk", &stat_buf) != 0) {
        ESP_LOGW(TAG, "/disk not mounted");
        httpd_resp_send(req, "{\"mounted\":false}", strlen("{\"mounted\":false}"));
        return ESP_OK;
    }

    ESP_LOGI(TAG, "/disk is mounted, getting storage info");

    uint64_t total_bytes = 0;
    uint64_t free_bytes  = 0;
    esp_err_t ret = esp_vfs_fat_info("/disk", &total_bytes, &free_bytes);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_vfs_fat_info failed: %s", esp_err_to_name(ret));
        httpd_resp_send(req, "{\"mounted\":true,\"error\":true}",
                        strlen("{\"mounted\":true,\"error\":true}"));
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Storage info: total=%llu, free=%llu", total_bytes, free_bytes);

    char json[256];
    snprintf(json, sizeof(json),
        "{\"mounted\":true,\"total_mb\":%llu,\"free_mb\":%llu}",
        total_bytes / (1024 * 1024),
        free_bytes  / (1024 * 1024));

    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

/* ========================================== 路由表 ============================================= */

static const web_route_t s_storage_routes[] = {
    { "/api/msc/filelist",     HTTP_GET,    storage_msc_filelist_handler,     false },
    { "/api/msc/upload",       HTTP_POST,   storage_msc_upload_handler,       false },
    { "/api/msc/download",     HTTP_GET,    storage_msc_download_handler,     false },
    { "/api/msc/preview",      HTTP_GET,    storage_msc_preview_handler,      false },
    { "/api/msc/batch-delete", HTTP_POST,   storage_msc_batch_delete_handler, false },
    { "/api/msc/restart",      HTTP_POST,   storage_msc_restart_handler,      false },
    { "/api/msc/mkdir",        HTTP_POST,   storage_msc_mkdir_handler,        false },
    { "/api/msc/rename",       HTTP_POST,   storage_msc_rename_handler,       false },
    { "/api/msc/move",         HTTP_POST,   storage_msc_move_handler,         false },
    { "/api/msc/status",       HTTP_GET,    storage_msc_status_handler,       false },
};

/* ========================================== 初始化 ============================================= */

void storage_web_handler_init(void)
{
    /* 初始化异步写入流水线（上传专用） */
    async_write_init();

    web_module_register_routes(s_storage_routes,
                               sizeof(s_storage_routes) / sizeof(s_storage_routes[0]));
}

/**
 * @brief 编译期自动注册本模块初始化函数到 Web 模块
 */
static void __attribute__((constructor, used)) storage_web_handler_auto_init(void)
{
    web_module_register_handler("storage", storage_web_handler_init);
}
