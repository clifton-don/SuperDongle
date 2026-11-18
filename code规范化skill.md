# SuperDongle 项目编码规范

## 适用范围

本项目 `SuperDongle` 下所有 C/C++ 源文件（`.c` / `.h`），基于 ESP-IDF v5.x 框架。

---

## 一、命名规范（snake_case）

### 1.1 文件级

| 元素 | 规范 | 示例 |
|------|------|------|
| 文件名 | `snake_case` | `wifi_manager.c`, `storage_module.h` |
| include guard | `#pragma once`（不使用 `#ifndef` 宏） | |
| 日志 TAG | `static const char *TAG = "[module_name]";` | `"[wifi_manage]"`, `"[web_module]"` |

### 1.2 类型

| 元素 | 规范 | 示例 |
|------|------|------|
| 结构体/联合体 | `snake_case_t` | `wifi_manage_config_t`, `write_job_t` |
| 枚举类型 | `snake_case_t` | `wifi_module_event_t`, `led_state_t` |
| 枚举值 | `UPPER_SNAKE_CASE` + 模块前缀 | `WIFI_MODULE_EVENT_STA_GOT_IP`, `LED_STATE_CONNECTED` |
| 函数指针类型 | `snake_case_cb_t` | `wifi_event_cb_t`, `web_get_status_cb_t` |

### 1.3 变量

| 作用域 | 规范 | 示例 |
|--------|------|------|
| 模块级静态变量 | `s_` 前缀 + `snake_case` | `s_wifi_cfg`, `s_http_server`, `s_write_queue` |
| FreeRTOS 句柄 | `s_` 前缀 + 描述性名词 | `s_mutex`, `s_timer`, `s_write_task` |
| 局部变量 | `snake_case`，允许常见缩写 | `ret`, `cnt`, `cfg`, `cap`, `buf`, `len` |
| 布尔变量 | `is_` / `has_` 前缀或描述性形容词 | `is_final`, `has_range`, `s_enabled`, `s_connecting` |
| 时间戳 | `_ts` 后缀 | `s_connected_ts`, `s_connect_failed_ts` |

### 1.4 函数

| 可见性 | 规范 | 示例 |
|--------|------|------|
| 公开 API | `snake_case` + 模块前缀 | `wifi_module_init()`, `storage_fatfs_sdcard_init()` |
| 内部静态函数 | `static` + `snake_case` | `internal_set_state()`, `async_write_send()` |
| 回调函数 | `snake_case` + `_cb` 后缀 | `timer_cb`, `wifi_event_cb`，或带模块名 `wifi_manage_on_wifi_event()` |
| HTTP handler | `snake_case` + `_handler` 后缀 | `web_module_msc_upload_handler()` |

### 1.5 宏

| 元素 | 规范 | 示例 |
|------|------|------|
| 常量宏 | `UPPER_SNAKE_CASE` | `ASYNC_BUF_SIZE`, `AP_GRACE_PERIOD_MS` |
| 功能宏 | 同常量宏 | `WEB_MAX_SCAN_RESULT`, `ERR_HOLD_TICKS` |

---

## 二、注释规范（Doxygen JavaDoc 风格）

### 2.1 文件头部

每个 `.c` / `.h` 文件以 `/** */` JavaDoc 块开头，含 4 个必要标签：

```c
/**
 * @file    wifi_manager.c
 * @brief   WiFi 管理层（状态机）实现
 *
 * 协调 wifi_module、storage_module、web_module，
 * 实现断开重连、已保存 WiFi 排序、用户手动连接优先级等策略。
 *
 * @date    2026-05-19
 * @author  Mr.Tang
 */
```

规则：
- `@file` — 文件名（与磁盘文件名一致）
- `@brief` — 一句话功能描述
- `@date` — 创建/最后修改日期
- `@author` — 修改人
- `@brief` 后可跟一段详细描述（无标签），用于说明设计意图、依赖关系
- **禁止口水话**：不出现「请注意」「对了」「其实就是」等口语词

### 2.2 公开 API 文档（.h 文件）

使用完整 Doxygen 标签集：

```c
/**
 * @brief 以 STA 模式连接指定 AP
 *
 * 若当前 WiFi 未处于 STA / APSTA 模式，接口会根据配置自动切换模式后再连接。
 * 调用成功仅表示已发起连接流程，连接结果通过 event_cb 上报。
 *
 * @param ssid     目标 AP SSID，必须非 NULL 且非空
 * @param password 目标 AP 密码，可为 NULL/空串表示开放网络
 * @return
 *      - ESP_OK                 已成功提交连接请求
 *      - ESP_ERR_INVALID_ARG    ssid 非法
 *      - ESP_ERR_INVALID_STATE  WiFi 模块未初始化或未启用 STA
 */
esp_err_t wifi_module_connect(const char *ssid, const char *password);
```

规则：
- `@brief` 必需
- `@param` 每个参数独立一行，含默认值说明
- `@return` 列出各返回值含义
- 可选的 `@note` / `@warning` / `@see`
- 描述文字使用句号结尾

### 2.3 内部函数文档（.c 文件 static 函数）

使用简化 Doxygen，至少含 `@brief`：

```c
/**
 * @brief 检查路径中是否包含路径遍历攻击（如 ../）
 *
 * @param path 待检查路径字符串
 * @return true=包含路径遍历，false=安全
 */
static bool is_path_traversal(const char *path)
```

对于公开 API 的实现，使用引用简写避免重复文档：

```c
/**
 * @brief 参见 wifi_module.h
 */
esp_err_t wifi_module_connect(const char *ssid, const char *password)
```

### 2.4 枚举与宏注释

枚举值与宏使用 `/**< */` 行内注释：

```c
typedef enum {
    WIFI_MODULE_EVENT_STA_CONNECTED,     /**< STA 已建立物理连接 */
    WIFI_MODULE_EVENT_STA_GOT_IP,        /**< STA 获取到 IPv4 地址 */
    WIFI_MODULE_EVENT_STA_DISCONNECTED,  /**< STA 意外断开 */
    WIFI_MODULE_EVENT_STA_CONNECT_FAILED,/**< STA 连接尝试失败 */
} wifi_module_event_t;

#define AP_GRACE_PERIOD_MS 5000  /**< 连接成功后 AP 公告时间（毫秒） */
```

枚举体本身使用 `/** @brief */` 类型级注释：

```c
/** @brief WiFi 模块事件类型，通过 event_cb 上报给上层管理者 */
typedef enum {
    ...
} wifi_module_event_t;
```

### 2.5 全局变量注释

模块级静态变量统一使用 `/**< */` 行内注释：

```c
static bool          s_wifi_inited = false;  /**< WiFi 模块初始化标志 */
static esp_netif_t  *s_sta_netif   = NULL;   /**< STA 网络接口句柄 */
static esp_netif_t  *s_ap_netif    = NULL;   /**< AP 网络接口句柄 */
```

### 2.6 禁止的注释形式

以下规则针对**对外文档**（文件/类型/函数/变量级注释）。**函数体内部的行内注释不受此限**（见 2.7 节）。

| 禁止 | 原因 | 替代方案 |
|------|------|----------|
| `//` 单行注释用于文档级（函数、类型、全局变量） | 非 Doxygen 标准 | `/**< */` 或 `/** @brief */` |
| `/* ... */` 用于文档级描述 | 不应作为正式文档 | `/** @brief ... */` |
| 口水话 | 不专业 | 技术性描述 |
| 表情符号 / 箭头艺术 | 不正式 | ASCII 图示仅在必要时使用 |

### 2.7 允许的注释形式

| 形式 | 用途 |
|------|------|
| `// ...` | **函数体内部行内注释**（解释实现细节、参数来源、设计取舍） |
| `/* ===...=== */` 长分隔符 | 模块级大段分隔（第一层，左右各 42 个 `=`） |
| `/* ---...--- */` 中等分隔符 | 功能子段分隔（第二层，左右各 20 个 `-`） |
| `/* --- Name --- */` 短分隔符 | 辅助分组分隔（第三层） |
| `/** @brief 参见 xxx.h */` | 实现文件引用头文件文档 |
| `/**< 描述 */` | 行内文档（枚举值、变量、宏） |
| `(void)var;` | 未使用参数消除警告 |

### 2.8 段落分隔注释规范

段落分隔注释（`/* === */` 块）可包含技术说明，但受以下约束：

**允许的内容**：

- 功能概述（如 `SPIFFS 文件系统 + A/B 分区管理`）
- 设计约束的技术性描述（如 `两边都需要读写 SPIFFS 的 NVS 信息，但互相不能依赖对方`）

**禁止的内容**：

- 反问/设问句式（如 `为什么这里也有一套 spiﬀs_ota_* 函数？` → 改为陈述句式）
- 口语化解释（如 `那是故意重复的` → 直接说明技术原因即可）
- 个人观点（如 `这是必要的设计选择` → 没必要强调「必要」）

**正反示例**：

```c
/* --- 反例：反问 + 口语化 --- */
/* 为什么这里也有一套 spiﬀs_ota_* 函数？
 * 那是故意重复的（因为组件隔离）。虽然函数看起来重复，但这是必要的选择。 */

/* --- 正例：陈述句式 + 技术原因 --- */
/* 为什么这里也有一套 spiffs_ota_* 函数？
 * storage_module 属于主项目，http_ota_manager 是独立组件，
 * 两者均需读写 SPIFFS NVS 信息但互相不可依赖。 */

/* --- 正例（更理想）：直接陈述，无需设问 --- */
/* storage_module 与 http_ota_manager 各自维护一套 spiffs_ota_* 函数，
 * 因组件隔离要求，两者不可相互依赖。 */
```

### 2.9 设计决策注释规范

当注释需要说明「为什么这样设计」时，遵循以下原则：

| 准则 | 说明 |
|------|------|
| 用陈述句，不用设问句 | 直接说「因组件隔离要求」而非「为什么…因为…」 |
| 只写技术事实，不写主观判断 | 写「两者均需读写 SPIFFS NVS 信息」而非「这是必要的」 |
| 引用数据或外部约束 | 写「A/B 分区各 1000K，uploadfs 默认写入 spiffs_1」而非「这样更安全」 |
| 一条注释只解决一个「为什么」 | 不要在一个块里同时解释 A/B 分区和代码重复两个不同的问题 |

---

## 三、模块设计模式

### 3.1 配置结构体 + 默认值宏

每个模块定义一个配置结构体和一个 `DEFAULT_CONFIG()` 宏：

```c
/** @brief WiFi 管理模块配置 */
typedef struct {
    const char *ap_ssid;             /**< 配网 AP SSID */
    const char *ap_password;         /**< 配网 AP 密码 */
    int         save_wifi_count;     /**< 已保存 WiFi 最大数量，<=0 时强制为 1 */
    long        reconnect_interval_ms; /**< 重连间隔（ms），<0 关闭自动重连 */
    wifi_event_cb_t wifi_event_cb;   /**< WiFi 事件回调，可为 NULL */
} wifi_manage_config_t;

/** @brief WiFi 管理模块默认配置 */
#define WIFI_MANAGE_DEFAULT_CONFIG() {                                   \
    .ap_ssid              = CONFIG_AP_SSID,                              \
    .ap_password          = CONFIG_AP_PASSWORD,                          \
    .save_wifi_count      = 5,                                           \
    .reconnect_interval_ms = 30000,                                      \
    .wifi_event_cb        = NULL,                                        \
}
```

规则：
- 所有字符串字段从 `CONFIG_XX` Kconfig 取值
- 回调字段默认 `NULL`
- 数值字段使用合理默认值
- 宏参数对齐（字段名对齐到冒号，值对齐到固定位置）

### 3.2 初始化模式

每个模块的初始化函数遵循统一模板：

```c
/**
 * @brief 参见 module.h
 */
esp_err_t module_init(const module_config_t *config)
{
    /* 1. 配置副本保存（NULL → 默认值） */
    if (config == NULL) {
        s_cfg = MODULE_DEFAULT_CONFIG();
    } else {
        s_cfg = *config;
    }

    /* 2. 幂等保护（可重复调用，不重复初始化） */
    if (s_inited) return ESP_OK;

    /* 3. 子模块初始化 */
    // ...

    /* 4. 资源创建（任务 / 队列 / 定时器 / 互斥锁） */
    // ...

    s_inited = true;
    ESP_LOGI(TAG, "module initialized");
    return ESP_OK;
}
```

### 3.3 事件回调模式

模块间通过回调函数指针解耦，禁止直接跨模块调用：

```c
/* --- 在底层模块中定义回调类型并对外暴露 --- */
typedef void (*wifi_event_cb_t)(wifi_module_event_t event);

typedef struct {
    wifi_event_cb_t event_cb;  /**< 事件回调，可为 NULL */
    // ...
} wifi_module_config_t;

/* --- 在底层模块中触发回调 --- */
static void module_handle_event(wifi_module_event_t event)
{
    if (s_cfg.event_cb) {
        s_cfg.event_cb(event);
    }
}

/* --- 在上层模块中注册回调 --- */
wifi_cfg.event_cb = wifi_manage_on_wifi_event;
```

### 3.4 FreeRTOS 任务创建模式

任务函数定义为 `static void task_func(void *arg)`，使用 `xTaskCreate()`：

```c
/** @brief 内存监控任务（调试用，每 10 秒打印一次堆信息） */
static void memory_monitor_task(void *pv_params)
{
    (void)pv_params;

    while (1) {
        ESP_LOGI(TAG, "free=%u min=%u",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024,
                 (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/* 创建任务 */
xTaskCreate(memory_monitor_task, "memory_monitor", 4096, NULL, 5, NULL);
```

规则：
- 任务优先级：I/O 密集型 > 管理型 > 监控型
- 任务栈大小优先设为 4096，根据实际峰值使用量调整
- `pv_params` 使用 `(void)` 消除未使用警告

---

## 四、代码组织

### 4.1 文件结构（从上到下）

```
1. @file 头注释块
2. #include 块（标准库 → FreeRTOS → ESP-IDF → 项目头文件，各组间空行分隔）
3. static const char *TAG = "[module_name]";
4. 宏定义（#define，字母序）
5. 类型定义（typedef struct/enum）
6. 模块级静态变量声明（字母序，同类型对齐）
7. 内部辅助函数（static，按调用层级自底向上排列）
8. 公开 API 函数（顺序与 .h 声明一致）
```

### 4.2 段落分隔

第一层（模块级大段）：长分隔符 `/* ========================================== Name ============================================= */`

```
/* ========================================== 配置宏 ============================================= */
```

第二层（功能子段）：中等分隔符 `/* -------------------- Name -------------------- */`

```
/* -------------------- 静态文件响应辅助 -------------------- */
```

第三层（辅助分组）：短分隔符 `/* --- Name --- */`

```
/* --- 异步写入辅助函数 --- */
```

规则：
- 各层分隔前后均留空行
- 分隔注释独占一行（不嵌入 `@brief` 等标签）
- 长分隔符左右 `=` 数量相等（建议 42 个），Section 名称居中
- 中等分隔符左右 `-` 数量相等（建议 20 个）

### 4.3 include 顺序

```
标准 C 头文件      → <ctype.h> <string.h> <sys/stat.h> ...
FreeRTOS 头文件    → "freertos/FreeRTOS.h" "freertos/task.h" ...
ESP-IDF 头文件     → "esp_log.h" "esp_wifi.h" "nvs_flash.h" ...
项目头文件         → "wifi_manager.h" "storage_module.h" ...
```

每组内按字母序排列，各组之间空行分隔。

---

## 五、错误处理

### 5.1 返回值约定

| 函数类型 | 返回值 | 示例 |
|----------|--------|------|
| 初始化/操作类 | `esp_err_t` | `ESP_OK`, `ESP_ERR_NO_MEM`, `ESP_ERR_INVALID_ARG` |
| 查询类 | `bool` 或直接类型 | `bool led_indicator_is_enabled(void)` |
| 通知/事件类 | `void` | `void led_indicator_file_event(led_file_event_t event)` |
| HTTP handler | `esp_err_t`（错误返回 `ESP_OK`） | 见 5.3 节 |

### 5.2 错误日志标准模板

```c
esp_err_t ret = esp_wifi_set_mode(mode);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(ret));
    return ret;
}
```

规则：
- 所有错误路径必须打日志
- 日志中必须使用 `esp_err_to_name(ret)` 输出错误名称
- 可容忍的重复初始化错误（如 `ESP_ERR_INVALID_STATE`）可降级为 `ESP_LOGW`

### 5.3 HTTP Handler 特殊规则

HTTP handler 中错误通过 `httpd_resp_send_err()` 返回客户端，函数本身返回 `ESP_OK`：

```c
static esp_err_t upload_handler(httpd_req_t *req)
{
    if (web_extract_filename(req, filename, sizeof(filename)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing filename");
        return ESP_OK;  /* 返回 ESP_OK，错误已通过 HTTP 响应传达 */
    }
    // ...
}
```

HTTP handler 命名统一为 `模块名_资源名_handler`，如 `web_module_msc_upload_handler`。

### 5.4 资源清理模式

出错时先释放已分配资源，再返回：

```c
wifi_config_t *list = heap_caps_malloc(...);
if (list == NULL) return ESP_ERR_NO_MEM;

esp_err_t ret = wifi_storage_load_all(list, &count);
if (ret != ESP_OK) {
    free(list);      /* 先释放 */
    return ret;      /* 再返回 */
}
```

---

## 六、内存分配

### 6.1 分配策略

| 场景 | 分配方式 | 说明 |
|------|----------|------|
| 热路径 DMA 缓冲（WiFi TX/RX、SD 读写） | `heap_caps_malloc(size, MALLOC_CAP_DMA)` | 内部 SRAM，高吞吐 |
| 冷路径大数据（JSON 解析、WiFi 扫描结果） | `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)` | PSRAM，省内部 RAM |
| 小型固定数组 | 栈分配 `char buf[512]` | 无需堆管理 |
| 需要清零的分配 | `heap_caps_calloc(n, size, caps)` | 替代 `calloc` |

### 6.2 通用规则

- 禁止直接使用 `malloc()` / `calloc()` / `free()`，统一走 `heap_caps_*`
- `heap_caps_*` 分配后必须检查 `NULL`
- 错误路径必须释放已分配资源
- 避免在热路径中分配/释放内存

---

## 七、FreeRTOS 模式

### 7.1 同步原语选用

| 场景 | 原语 | 示例 |
|------|------|------|
| 互斥保护共享资源 | `SemaphoreHandle_t` + `xSemaphoreCreateMutex()` | 保护 RMT / 硬件寄存器 |
| 任务间数据传递 | `QueueHandle_t` + `xQueueCreate()` | 异步写入作业队列 |
| 一对多事件通知 | `EventGroupHandle_t` + `xEventGroupCreate()` | 最后一个 chunk 写入完成通知 |
| 定时周期任务 | `TimerHandle_t` + `xTimerCreate()` | LED 闪烁 |

### 7.2 队列传递结构体模式

```c
typedef struct {
    char   *buf;
    size_t  size;
    int     fd;
    bool    is_final;
    EventGroupHandle_t done_evt;
} write_job_t;

/* 创建队列 */
s_write_queue = xQueueCreate(1, sizeof(write_job_t));

/* 发送方：阻塞发送 */
xQueueSend(s_write_queue, &job, portMAX_DELAY);

/* 接收方：阻塞接收 */
if (xQueueReceive(s_write_queue, &job, portMAX_DELAY) == pdTRUE) {
    /* 处理 job */;
}
```

---

## 八、NVS 命名空间约定

| 命名空间 | 用途 | 键 |
|----------|------|-----|
| `"superdongle"` | 全局设置 | `"led_enabled"` (u8), `"mdns_name"` (str) |
| `"wifi_store"` | WiFi 配置持久化 | `"wifi_list"` (blob) |
| `"spiffs_ota"` | SPIFFS A/B 分区管理 | `"active"` (u8) |

---

## 九、禁止事项（Anti-patterns）

| 禁止 | 原因 | 替代方案 |
|------|------|----------|
| 口水话注释 | 不专业 | 技术性描述 |
| `//` 单行注释用于文档 | 非 Doxygen 标准 | `/**< */` 或 `/** @brief */` |
| 魔法数字（除 0/1/NULL） | 可读性差 | 命名宏 |
| 头文件包含实现细节 | 耦合过重 | `.h` 只放接口 |
| 跨模块直接函数调用 | 循环依赖 | 回调函数解耦 |
| 任务中 `while(1)` 无延迟 | 占满 CPU | `vTaskDelay()` 或阻塞式 API |
| 全局变量（非 `static`） | 命名空间污染 | 模块级 `s_` 静态变量 |
| `malloc`/`free` 裸调用 | 无法控制分配位置 | `heap_caps_*` 系列 |
| HTTP handler 返回非 `ESP_OK` | 客户端收到 500 | 返回 `ESP_OK`，通过 `httpd_resp_send_err` 传达 |

---

## 十、参考模板

### 10.1 头文件模板

```c
/**
 * @file    module_name.h
 * @brief   模块一句话概述
 *
 * 模块详细描述（功能、依赖、使用场景）。
 *
 * @date    YYYY-MM-DD
 * @author  YourName
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================== 类型定义 ============================================= */

/** @brief 模块事件类型（通过 event_cb 上报） */
typedef enum {
    MODULE_EVT_A,  /**< 事件 A 描述 */
    MODULE_EVT_B,  /**< 事件 B 描述 */
} module_event_t;

/** @brief 模块配置结构体 */
typedef struct {
    const char         *field_str;   /**< 字符串配置项，默认来自 Kconfig */
    int                 field_num;   /**< 数值配置项 */
    void (*event_cb)(module_event_t); /**< 事件回调，可为 NULL */
} module_config_t;

/** @brief 模块默认配置 */
#define MODULE_DEFAULT_CONFIG() {                                        \
    .field_str = CONFIG_XX_STRING,                                       \
    .field_num = 10,                                                     \
    .event_cb  = NULL,                                                   \
}

/* ========================================== 公共 API ============================================= */

/**
 * @brief 模块初始化
 *
 * @param config 配置指针，NULL 为使用 MODULE_DEFAULT_CONFIG
 * @return
 *      - ESP_OK      初始化成功
 *      - ESP_ERR_NO_MEM 内存不足
 */
esp_err_t module_init(const module_config_t *config);

#ifdef __cplusplus
}
#endif
```

### 10.2 实现文件模板

```c
/**
 * @file    module_name.c
 * @brief   模块一句话概述
 *
 * 模块详细描述。
 *
 * @date    YYYY-MM-DD
 * @author  YourName
 */

#include "module_name.h"

#include <string.h>
#include "esp_log.h"

static const char *TAG = "[module_name]";

/* ========================================== 配置宏 ============================================= */

/** @brief 宏描述 */
#define MODULE_TIMEOUT_MS  5000

/* ========================================== 模块级静态变量 ============================================= */

static bool          s_inited = false;  /**< 初始化标志 */
static module_config_t s_cfg = {0};      /**< 配置副本 */

/* ========================================== 内部函数 ============================================= */

/**
 * @brief 触发事件回调
 *
 * @param event 事件类型
 */
static void module_dispatch_event(module_event_t event)
{
    if (s_cfg.event_cb) {
        s_cfg.event_cb(event);
    }
}

/* ========================================== 公共 API ============================================= */

/**
 * @brief 参见 module_name.h
 */
esp_err_t module_init(const module_config_t *config)
{
    if (config == NULL) {
        s_cfg = MODULE_DEFAULT_CONFIG();
    } else {
        s_cfg = *config;
    }

    if (s_inited) return ESP_OK;

    /* 初始化逻辑 */

    s_inited = true;
    ESP_LOGI(TAG, "module initialized");
    return ESP_OK;
}
```

## 十一、校验清单

在对已有文件进行规范化时，逐项检查以下清单。全部通过方可认为合规。

### 11.1 文件头检查

| # | 检查项 | 通过标准 |
|---|--------|----------|
| 1.1 | Doxygen 文件头 | `/** @file @brief @date @author */` 四项齐全 |
| 1.2 | 无口水话 | 无「请注意」「对了」「其实就是」「总之」等口语词 |
| 1.3 | 文件名准确 | `@file` 与实际磁盘文件名一致 |
| 1.4 | 文件级注释为技术描述 | 描述功能与设计意图，不含个人观点 |

### 11.2 命名检查

| # | 检查项 | 通过标准 |
|---|--------|----------|
| 2.1 | include guard | `.h` 文件使用 `#pragma once`，无 `#ifndef` |
| 2.2 | 日志 TAG | `static const char *TAG = "[module_name]";` |
| 2.3 | 模块级静态变量 | `s_` 前缀，`/**< */` 行内注释 |
| 2.4 | 函数命名 | 公开函数带模块前缀，内部函数 `static` |

### 11.3 注释检查

| # | 检查项 | 通过标准 |
|---|--------|----------|
| 3.1 | 枚举值 | 全部 `/**< */` 行内注释 |
| 3.2 | 结构体成员 | 全部 `/**< */` 行内注释 |
| 3.3 | 公开 API（.h） | `/** @brief @param @return */` 完整标签 |
| 3.4 | 公开 API 实现（.c） | `/** @brief 参见 xxx.h */` 引用简写 |
| 3.5 | 内部函数 | 至少 `/** @brief */`，有参数则加 `@param @return` |
| 3.6 | 无 `//` 用于文档 | 全部替换为 `/** */` 或 `/**< */` |
| 3.7 | 无口水话 | 全文搜索（反问句/口语词/个人观点） |
| 3.8 | 段落分隔无设问 | `/* === */` 块中使用陈述句式 |

### 11.4 代码组织检查

| # | 检查项 | 通过标准 |
|---|--------|----------|
| 4.1 | include 顺序 | 标准 C → FreeRTOS → ESP-IDF → 项目头文件，各组空行 |
| 4.2 | 段落分隔 | 第一层 `/* === */`，第二层 `/* --- */` |
| 4.3 | 文件结构 | 按 4.1 节顺序排列（宏 → 类型 → 静态变量 → 内部函数 → 公开 API） |

### 11.5 错误处理检查

| # | 检查项 | 通过标准 |
|---|--------|----------|
| 5.1 | 错误路径有日志 | 所有 `ret != ESP_OK` 均含 `ESP_LOGE(TAG, "...")` |
| 5.2 | 日志含错误名 | 使用 `esp_err_to_name(ret)` |
| 5.3 | 资源释放 | 错误路径先 `free()` / `nvs_close()` 再 `return` |

---

*本文档基于 SuperDongle 项目代码规范实践整理，适用于本项目所有源文件的创建与维护。版本迭代请同步更新 `@date`。*