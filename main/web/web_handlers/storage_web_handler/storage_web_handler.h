/**
 * @file    storage_web_handler.h
 * @brief   SD 卡文件管理 Web handler（MSC 存储相关）
 *
 * @date    2026-07-24
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册 SD 卡文件管理相关的 HTTP 路由到 Web 服务
 *
 * 注册以下路由：
 *   - GET  /api/msc/filelist      → 文件列表
 *   - POST /api/msc/upload        → 文件上传
 *   - GET  /api/msc/download      → 文件下载
 *   - GET  /api/msc/preview       → 文件预览（Range 支持）
 *   - POST /api/msc/batch-delete  → 批量删除
 *   - POST /api/msc/restart       → 软重启 MSC
 *   - POST /api/msc/mkdir         → 新建文件夹
 *   - POST /api/msc/rename        → 重命名
 *   - POST /api/msc/move          → 移动文件
 *   - GET  /api/msc/status        → 存储状态查询
 */
void storage_web_handler_init(void);

#ifdef __cplusplus
}
#endif
