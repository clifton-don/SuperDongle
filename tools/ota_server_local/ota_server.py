#!/usr/bin/env python3
"""
本地 OTA 固件服务器 - 用于 ESP32 本地 OTA 测试
界面同 OTAweb/index.php 原版风格

用法:
    python ota_server.py              # 默认 0.0.0.0:8000
    python ota_server.py --port 8080  # 自定义端口
"""

import http.server
import json
import os
import re
import time
import socket
import argparse
import subprocess
import shutil
import sys
import ctypes
import webbrowser
from pathlib import Path

BASE_DIR = Path(__file__).parent.resolve()
FIRMWARE_DIR = BASE_DIR / "firmware"
FIRMWARE_DIR.mkdir(exist_ok=True)

# 持久化记住上次选择的文件夹路径
CONFIG_FILE = BASE_DIR / ".ota_server_config.json"
_last_selected_folder = None


def _load_config():
    global _last_selected_folder
    try:
        if CONFIG_FILE.exists():
            data = json.loads(CONFIG_FILE.read_text(encoding="utf-8"))
            _last_selected_folder = data.get("last_folder") or None
    except Exception:
        pass


def _save_config():
    try:
        data = {}
        if _last_selected_folder:
            data["last_folder"] = _last_selected_folder
        CONFIG_FILE.write_text(json.dumps(data, ensure_ascii=False), encoding="utf-8")
    except Exception:
        pass


def scan_folder_for_firmware(folder_path):
    """扫描文件夹，返回 (system_firmware_path, web_firmware_path)"""
    folder = Path(folder_path)
    if not folder.exists() or not folder.is_dir():
        return None, None

    system_fw = None
    web_fw = None
    for f in folder.iterdir():
        if not f.is_file() or not f.name.lower().endswith(".bin"):
            continue
        lower_name = f.name.lower()
        if lower_name == "superdongle.bin":
            system_fw = f
        elif "spiffs" in lower_name:
            web_fw = f
    return system_fw, web_fw


def copy_firmware_to_storage(src_path):
    """复制固件到存储目录，返回文件名，spiffs 相关文件统一重命名为 spiffs.bin"""
    name = src_path.name
    # spiffs 文件统一重命名
    if "spiffs" in name.lower():
        name = "spiffs.bin"
    
    dest_path = FIRMWARE_DIR / name
    # 如果目标已存在则覆盖
    shutil.copy2(src_path, dest_path)
    log_name = f"{src_path.name} → {name}" if name != src_path.name else src_path.name
    print(f"  ✓ 复制固件: {log_name}")
    return name

# ═══════════════════════════════════════════════════════════
# 管理页面 HTML（复用 OTAweb/index.php 原版设计）
# ═══════════════════════════════════════════════════════════
MANAGEMENT_HTML = r"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OTA固件管理</title>
    <style>
        * {margin: 0; padding: 0; box-sizing: border-box;}
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 20px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            overflow: hidden;
        }
        header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
        }
        h1 {font-size: 2em; margin-bottom: 10px;}
        .subtitle {font-size: 0.9em; opacity: 0.85;}
        main {padding: 30px;}
        .section {
            background: #f9fafb;
            border-radius: 12px;
            padding: 25px;
            margin-bottom: 25px;
        }
        .section h2 {
            color: #667eea;
            margin-bottom: 20px;
            font-size: 1.5em;
        }
        .form-group {margin-bottom: 20px;}
        .form-group label {
            display: block;
            margin-bottom: 8px;
            font-weight: 600;
            color: #374151;
        }
        .form-group input[type="text"],
        .form-group textarea,
        .form-control {
            width: 100%;
            padding: 12px;
            border: 2px solid #e5e7eb;
            border-radius: 8px;
            font-size: 1em;
            background: white;
            color: #374151;
            box-sizing: border-box;
        }
        .form-group input:focus,
        .form-group textarea:focus {
            outline: none;
            border-color: #667eea;
        }
        .inline-row {
            display: flex;
            align-items: center;
            gap: 12px;
        }
        .inline-row input[type="text"] {flex: 1;}
        .checkbox-group {
            display: flex;
            align-items: center;
            gap: 10px;
            white-space: nowrap;
        }
        .checkbox-group input[type="checkbox"] {
            width: 20px;
            height: 20px;
            cursor: pointer;
        }
        .checkbox-group label {
            margin: 0 !important;
            font-weight: 400 !important;
            cursor: pointer;
        }
        .btn {
            padding: 12px 24px;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            font-size: 1em;
            font-weight: 500;
            transition: all 0.3s;
        }
        .btn-primary {background: #667eea; color: white;}
        .btn-primary:hover {background: #5568d3;}
        .btn-success {background: #10b981; color: white;}
        .btn-success:hover {background: #059669;}
        .btn-danger {background: #ef4444; color: white;}
        .btn-danger:hover {background: #dc2626;}
        .btn-secondary {background: #6b7280; color: white;}
        .btn-secondary:hover {background: #4b5563;}
        .btn-small {padding: 6px 12px; font-size: 0.9em;}
        .upload-area {
            border: 3px dashed #d1d5db;
            border-radius: 12px;
            padding: 40px;
            text-align: center;
            cursor: pointer;
            transition: all 0.3s;
        }
        .upload-area:hover {
            border-color: #667eea;
            background: #f3f4f6;
        }
        .upload-area.dragover {
            border-color: #667eea;
            background: #e0e7ff;
        }
        .file-input {display: none;}
        .firmware-list {margin-top: 20px;}
        .firmware-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 15px;
            background: white;
            border: 2px solid #e5e7eb;
            border-radius: 8px;
            margin-bottom: 10px;
        }
        .firmware-info {flex: 1;}
        .firmware-name {font-weight: 600; color: #374151; margin-bottom: 5px;}
        .firmware-meta {font-size: 0.9em; color: #6b7280;}
        .firmware-actions {display: flex; gap: 8px;}
        .alert {
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 20px;
        }
        .alert-success {background: #d1fae5; color: #065f46; border: 2px solid #10b981;}
        .alert-error {background: #fee2e2; color: #991b1b; border: 2px solid #ef4444;}
        .alert-info {background: #dbeafe; color: #1e40af; border: 2px solid #3b82f6;}
        .config-preview {
            background: #1f2937;
            color: #f3f4f6;
            padding: 20px;
            border-radius: 8px;
            font-family: 'Courier New', monospace;
            font-size: 0.9em;
            white-space: pre-wrap;
            word-break: break-all;
            overflow-x: auto;
            margin-top: 15px;
        }
        .progress-bar {
            width: 100%;
            height: 30px;
            background: #e5e7eb;
            border-radius: 15px;
            overflow: hidden;
            margin: 15px 0;
            display: none;
        }
        .progress-fill {
            height: 100%;
            background: linear-gradient(90deg, #667eea, #764ba2);
            width: 0%;
            transition: width 0.3s;
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-weight: 600;
        }
        .hint {font-size: 0.85em; color: #6b7280; margin-top: 8px;}
        .hint code {background: #e5e7eb; padding: 2px 6px; border-radius: 4px; font-size: 0.95em;}
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>📦 OTA固件管理</h1>
            <div class="subtitle">本地测试服务器 — ESP32 固件更新</div>
        </header>

        <main>
            <!-- 当前配置 -->
            <div class="section">
                <h2>📝 当前OTA配置</h2>
                <div class="alert alert-info">
                    <strong>配置文件路径：</strong> /firmware/version.json<br>
                    <strong>设备访问地址：</strong> <span id="deviceUrl"></span>
                </div>
                <p class="hint">
                    将上述"设备访问地址"填写到 ESP32 网页开发者模式下的 <strong>OTA Server URL</strong> 处即可。
                </p>

                <form id="configForm">
                    <div class="form-group">
                        <label>固件版本号 *</label>
                        <input type="text" id="cfgVersion" required placeholder="1.0.0">
                    </div>

                    <div class="form-group">
                        <label>固件下载URL</label>
                        <div class="inline-row">
                            <input type="text" id="cfgUrl" placeholder="http://192.168.x.x:8000/firmware/firmware.bin">
                            <button type="button" class="btn btn-success btn-small" onclick="document.getElementById('cfgUrlFile').click()">📁 选择文件</button>
                            <input type="file" id="cfgUrlFile" class="file-input" accept=".bin" onchange="pickConfigFile(this, 'cfgUrl')">
                            <div class="checkbox-group">
                                <input type="checkbox" id="cfgFirmwareEnabled" checked>
                                <label for="cfgFirmwareEnabled">更新系统固件</label>
                            </div>
                        </div>
                    </div>

                    <div class="form-group">
                        <label>文件系统镜像URL</label>
                        <div class="inline-row">
                            <input type="text" id="cfgSpiffsUrl" placeholder="http://192.168.x.x:8000/firmware/spiffs.bin">
                            <button type="button" class="btn btn-success btn-small" onclick="document.getElementById('cfgSpiffsFile').click()">📁 选择文件</button>
                            <input type="file" id="cfgSpiffsFile" class="file-input" accept=".bin" onchange="pickConfigFile(this, 'cfgSpiffsUrl')">
                            <div class="checkbox-group">
                                <input type="checkbox" id="cfgSpiffsEnabled" checked>
                                <label for="cfgSpiffsEnabled">更新网页文件</label>
                            </div>
                        </div>
                    </div>

                    <p class="hint" style="margin-top: -10px;">※ 至少勾选一项更新内容，且对应URL不能为空</p>

                    <div class="form-group">
                        <label>更新说明</label>
                        <textarea id="cfgDescription" rows="3" placeholder="修复了若干bug，优化了性能"></textarea>
                    </div>

                    <div class="form-group">
                        <div class="checkbox-group">
                            <input type="checkbox" id="cfgForce">
                            <label for="cfgForce" style="margin: 0;">强制更新（即使设备版本号高于或等于此版本也会提示更新）</label>
                        </div>
                    </div>

                    <button type="submit" class="btn btn-primary">💾 保存配置</button>
                    <button type="button" class="btn btn-secondary" onclick="previewConfig()">👁️ 预览JSON</button>
                </form>

                <div id="configPreview" class="config-preview" style="display: none;"></div>
            </div>

            <!-- 上传固件 -->
            <div class="section">
                <h2>⬆️ 上传固件</h2>
                <div class="upload-area" id="uploadArea" onclick="document.getElementById('fileInput').click()">
                    <div style="font-size: 3em; margin-bottom: 10px;">📁</div>
                    <div style="font-size: 1.2em; color: #374151; margin-bottom: 5px;">点击或拖拽文件到此处上传</div>
                    <div style="color: #6b7280; font-size: 0.9em;">支持 .bin 格式的固件文件，最大 10MB</div>
                </div>
                <input type="file" id="fileInput" class="file-input" accept=".bin" onchange="uploadFirmware(this.files[0])">

                <div class="progress-bar" id="progressBar">
                    <div class="progress-fill" id="progressFill">0%</div>
                </div>

                <div id="uploadMessage"></div>
            </div>

            <!-- 固件列表 -->
            <div class="section">
                <h2>📋 已上传的固件</h2>
                <div style="margin-bottom: 15px; display: flex; gap: 10px; flex-wrap: wrap;">
                    <label class="checkbox-group" style="cursor: pointer;">
                        <input type="checkbox" id="selectAll" onchange="toggleSelectAll()">
                        <span style="margin: 0 !important;">全选</span>
                    </label>
                    <button class="btn btn-danger btn-small" id="deleteSelected" onclick="deleteSelected()" disabled>🗑️ 删除选中</button>
                </div>
                <div class="firmware-list" id="firmwareList">
                    <div class="alert alert-info">暂无固件文件</div>
                </div>
            </div>
            
            <!-- 从服务器文件夹导入 -->
            <div class="section">
                <h2>📂 从服务器本地文件夹导入</h2>
                <p style="margin-bottom: 12px; color: #6b7280;">选择服务器本地的文件夹，自动识别 SuperDongle.bin（系统固件）和 spiffs 相关文件（网页固件）</p>
                <button class="btn btn-primary" onclick="selectServerFolder()">📂 选择文件夹并导入</button>
            </div>
        </main>
    </div>

    <script>
    const DEVICE_URL = '<span id="deviceUrlInner"></span>';
    let selectedFiles = new Set(); // 保存选中的文件名

    window.addEventListener('DOMContentLoaded', () => {
        document.getElementById('deviceUrl').innerHTML = '<code>' + location.origin + '/firmware/version.json' + '</code>';
        loadConfig();
        loadFiles();
    });

    function showMessage(type, message) {
        const msgDiv = document.getElementById('uploadMessage');
        msgDiv.className = 'alert alert-' + type;
        msgDiv.textContent = message;
        msgDiv.style.display = 'block';
        setTimeout(() => { msgDiv.style.display = 'none'; }, 5000);
    }

    // 固件类型检测
    function getFirmwareType(filename) {
        const lower = filename.toLowerCase();
        if (lower === 'superdongle.bin') return 'system';
        if (lower.includes('spiffs')) return 'web';
        return 'other';
    }

    // 更新删除选中按钮状态
    function updateDeleteSelectedBtn() {
        const btn = document.getElementById('deleteSelected');
        btn.disabled = selectedFiles.size === 0;
    }

    // 切换全选
    function toggleSelectAll() {
        const selectAll = document.getElementById('selectAll');
        const checkboxes = document.querySelectorAll('.firmware-checkbox');
        if (selectAll.checked) {
            checkboxes.forEach(cb => {
                cb.checked = true;
                selectedFiles.add(cb.value);
            });
        } else {
            checkboxes.forEach(cb => {
                cb.checked = false;
            });
            selectedFiles.clear();
        }
        updateDeleteSelectedBtn();
    }

    // 切换单个文件选择
    function toggleFileSelect(filename, cb) {
        if (cb.checked) {
            selectedFiles.add(filename);
        } else {
            selectedFiles.delete(filename);
        }
        // 更新全选框状态
        const checkboxes = document.querySelectorAll('.firmware-checkbox');
        const allChecked = Array.from(checkboxes).every(cb => cb.checked);
        document.getElementById('selectAll').checked = allChecked;
        updateDeleteSelectedBtn();
    }

    // 删除选中的文件
    async function deleteSelected() {
        if (selectedFiles.size === 0) return;
        if (!confirm(`确定要删除选中的 ${selectedFiles.size} 个文件吗？`)) return;
        try {
            const res = await fetch('/api/delete_batch', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ filenames: Array.from(selectedFiles) }),
            });
            const data = await res.json();
            if (data.success) {
                showMessage('success', `✅ 成功删除 ${data.deleted_count} 个文件`);
                selectedFiles.clear();
                document.getElementById('selectAll').checked = false;
                updateDeleteSelectedBtn();
                loadFiles();
            } else {
                showMessage('error', '❌ 删除失败: ' + (data.message || data.error));
            }
        } catch(e) {
            showMessage('error', '❌ 删除失败: ' + e.message);
        }
    }

    // 从文件夹导入固件
    async function importFromFolder(input) {
        const files = input.files;
        if (!files || files.length === 0) return;
        
        let firmwareFile = null;
        let spiffsFile = null;
        let otherFiles = [];
        
        for (const file of files) {
            if (!file.name.toLowerCase().endsWith('.bin')) continue;
            const type = getFirmwareType(file.name);
            if (type === 'system') {
                firmwareFile = file;
            } else if (type === 'web') {
                spiffsFile = file;
            } else {
                otherFiles.push(file);
            }
        }
        
        if (!firmwareFile && !spiffsFile && otherFiles.length === 0) {
            showMessage('error', '❌ 未找到 .bin 固件文件');
            input.value = '';
            return;
        }
        
        // 构建要上传的文件列表
        let filesToUpload = [];
        if (firmwareFile) filesToUpload.push(firmwareFile);
        if (spiffsFile) filesToUpload.push(spiffsFile);
        filesToUpload = filesToUpload.concat(otherFiles);
        
        // 逐个上传
        let uploadedCount = 0;
        let failedCount = 0;
        const progressBar = document.getElementById('progressBar');
        const progressFill = document.getElementById('progressFill');
        progressBar.style.display = 'block';
        
        for (let i = 0; i < filesToUpload.length; i++) {
            const file = filesToUpload[i];
            const percent = Math.round(((i + 1) / filesToUpload.length) * 100);
            progressFill.style.width = percent + '%';
            progressFill.textContent = `上传中: ${i + 1}/${filesToUpload.length}`;
            
            try {
                const formData = new FormData();
                formData.append('firmware', file);
                const res = await fetch('/api/upload', {
                    method: 'POST',
                    body: formData,
                });
                if (res.ok) {
                    uploadedCount++;
                } else {
                    failedCount++;
                }
            } catch(e) {
                failedCount++;
            }
        }
        
        progressBar.style.display = 'none';
        
        if (uploadedCount > 0) {
            let msg = `✅ 成功导入 ${uploadedCount} 个文件`;
            if (failedCount > 0) msg += `，${failedCount} 个失败`;
            showMessage('success', msg);
            loadFiles();
            
            // 尝试自动填入配置
            if (firmwareFile || spiffsFile) {
                setTimeout(() => {
                    updateConfigFromUploaded(firmwareFile?.name, spiffsFile?.name);
                }, 300);
            }
        } else {
            showMessage('error', '❌ 导入失败');
        }
        
        input.value = '';
    }

    // 上传完成后自动填入配置
    async function updateConfigFromUploaded(firmwareName, spiffsName) {
        if (firmwareName) {
            const url = location.origin + '/firmware/' + firmwareName;
            document.getElementById('cfgUrl').value = url;
        }
        if (spiffsName) {
            const url = location.origin + '/firmware/' + spiffsName;
            document.getElementById('cfgSpiffsUrl').value = url;
        }
    }

    // 选择服务器本地文件夹
    async function selectServerFolder() {
        try {
            const firmwareEnabled = document.getElementById('cfgFirmwareEnabled').checked;
            const spiffsEnabled = document.getElementById('cfgSpiffsEnabled').checked;
            const res = await fetch('/api/select_folder', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ firmware_enabled: firmwareEnabled, spiffs_enabled: spiffsEnabled })
            });
            const data = await res.json();
            
            if (data.success) {
                let msg = '✅ 导入成功：';
                if (data.system_firmware) msg += ` 系统固件(${data.system_firmware})`;
                if (data.web_firmware) msg += ` 网页固件(${data.web_firmware})`;
                showMessage('success', msg);
                
                // 自动填入配置
                if (data.system_firmware) {
                    document.getElementById('cfgUrl').value = location.origin + '/firmware/' + data.system_firmware;
                }
                if (data.web_firmware) {
                    document.getElementById('cfgSpiffsUrl').value = location.origin + '/firmware/' + data.web_firmware;
                }
                
                // 刷新固件列表 + 配置预览
                loadFiles();
                loadConfig();
            } else {
                showMessage('error', '❌ ' + (data.message || '选择文件夹失败'));
            }
        } catch (e) {
            showMessage('error', '❌ 选择文件夹失败：' + e.message);
        }
    }

    // 加载当前配置
    async function loadConfig() {
        try {
            const r = await fetch('/firmware/version.json');
            if (!r.ok) return;
            const data = await r.json();
            document.getElementById('cfgVersion').value = data.version || '';
            document.getElementById('cfgUrl').value = data.firmware_url || '';
            document.getElementById('cfgSpiffsUrl').value = data.spiffs_url || '';
            document.getElementById('cfgDescription').value = data.description || '';
            document.getElementById('cfgFirmwareEnabled').checked = data.firmware_enabled !== false;
            document.getElementById('cfgSpiffsEnabled').checked = data.spiffs_enabled !== false;
            document.getElementById('cfgForce').checked = !!data.force;
        } catch(e) { /* 首次加载 version.json 可能不存在 */ }
    }

    // 保存配置
    document.getElementById('configForm').addEventListener('submit', async (e) => {
        e.preventDefault();
        const config = {
            version: document.getElementById('cfgVersion').value,
            firmware_url: document.getElementById('cfgUrl').value,
            spiffs_url: document.getElementById('cfgSpiffsUrl').value,
            description: document.getElementById('cfgDescription').value,
            force: document.getElementById('cfgForce').checked,
            firmware_enabled: document.getElementById('cfgFirmwareEnabled').checked,
            spiffs_enabled: document.getElementById('cfgSpiffsEnabled').checked,
        };
        try {
            const res = await fetch('/api/save_config', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(config),
            });
            const data = await res.json();
            if (data.success) {
                showMessage('success', '✅ 配置保存成功！');
            } else {
                showMessage('error', '❌ 保存失败: ' + (data.message || data.error));
            }
        } catch(e) {
            showMessage('error', '❌ 保存失败: ' + e.message);
        }
    });

    // 预览 JSON
    function previewConfig() {
        const config = {
            version: document.getElementById('cfgVersion').value,
            firmware_url: document.getElementById('cfgUrl').value,
            spiffs_url: document.getElementById('cfgSpiffsUrl').value,
            description: document.getElementById('cfgDescription').value,
            force: document.getElementById('cfgForce').checked,
            firmware_enabled: document.getElementById('cfgFirmwareEnabled').checked,
            spiffs_enabled: document.getElementById('cfgSpiffsEnabled').checked,
        };
        const preview = document.getElementById('configPreview');
        preview.textContent = JSON.stringify(config, null, 4);
        preview.style.display = 'block';
    }

    // 从配置表单选择文件上传，自动回填URL
    async function pickConfigFile(input, urlFieldId) {
        const file = input.files[0];
        if (!file) return;
        if (!file.name.endsWith('.bin')) {
            showMessage('error', '❌ 只支持 .bin 格式的固件文件');
            input.value = '';
            return;
        }

        const formData = new FormData();
        formData.append('firmware', file);

        const progressBar = document.getElementById('progressBar');
        const progressFill = document.getElementById('progressFill');
        progressBar.style.display = 'block';

        try {
            const xhr = new XMLHttpRequest();
            xhr.upload.addEventListener('progress', (e) => {
                if (e.lengthComputable) {
                    const percent = Math.round((e.loaded / e.total) * 100);
                    progressFill.style.width = percent + '%';
                    progressFill.textContent = percent + '%';
                }
            });
            xhr.addEventListener('load', () => {
                progressBar.style.display = 'none';
                if (xhr.status === 200) {
                    const data = JSON.parse(xhr.responseText);
                    if (data.success) {
                        const url = location.origin + '/firmware/' + data.filename;
                        document.getElementById(urlFieldId).value = url;
                        showMessage('success', '✅ 上传成功，URL 已自动填入');
                        loadFiles();
                    } else {
                        showMessage('error', '❌ 上传失败: ' + (data.message || data.error));
                    }
                } else {
                    showMessage('error', '❌ 上传失败');
                }
            });
            xhr.addEventListener('error', () => {
                progressBar.style.display = 'none';
                showMessage('error', '❌ 上传失败');
            });
            xhr.open('POST', '/api/upload');
            xhr.send(formData);
        } catch(e) {
            progressBar.style.display = 'none';
            showMessage('error', '❌ 上传失败: ' + e.message);
        }
        input.value = '';
    }

    // 上传固件
    async function uploadFirmware(file) {
        if (!file) return;
        if (!file.name.endsWith('.bin')) {
            showMessage('error', '❌ 只支持 .bin 格式的固件文件');
            return;
        }

        const formData = new FormData();
        formData.append('firmware', file);

        const progressBar = document.getElementById('progressBar');
        const progressFill = document.getElementById('progressFill');
        progressBar.style.display = 'block';

        try {
            const xhr = new XMLHttpRequest();
            xhr.upload.addEventListener('progress', (e) => {
                if (e.lengthComputable) {
                    const percent = Math.round((e.loaded / e.total) * 100);
                    progressFill.style.width = percent + '%';
                    progressFill.textContent = percent + '%';
                }
            });
            xhr.addEventListener('load', () => {
                if (xhr.status === 200) {
                    const data = JSON.parse(xhr.responseText);
                    if (data.success) {
                        showMessage('success', '✅ 固件上传成功！');
                        loadFiles();
                    } else {
                        showMessage('error', '❌ 上传失败: ' + (data.message || data.error));
                    }
                } else {
                    showMessage('error', '❌ 上传失败');
                }
                progressBar.style.display = 'none';
            });
            xhr.addEventListener('error', () => {
                showMessage('error', '❌ 上传失败');
                progressBar.style.display = 'none';
            });
            xhr.open('POST', '/api/upload');
            xhr.send(formData);
        } catch(e) {
            showMessage('error', '❌ 上传失败: ' + e.message);
            progressBar.style.display = 'none';
        }
    }

    // 删除固件
    async function deleteFirmware(filename) {
        if (!confirm('确定要删除固件 ' + filename + ' 吗？')) return;
        try {
            const res = await fetch('/api/delete', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ filename: filename }),
            });
            const data = await res.json();
            if (data.success) {
                showMessage('success', '✅ 删除成功！');
                loadFiles();
            } else {
                showMessage('error', '❌ 删除失败: ' + (data.message || data.error));
            }
        } catch(e) {
            showMessage('error', '❌ 删除失败: ' + e.message);
        }
    }

    // 复制 URL
    function copyUrl(filename) {
        const url = location.origin + '/firmware/' + filename;
        if (navigator.clipboard && typeof navigator.clipboard.writeText === 'function') {
            navigator.clipboard.writeText(url).then(() => {
                showMessage('success', '✅ 已复制: ' + url);
            }).catch(() => { showMessage('error', '❌ 复制失败'); });
            return;
        }
        const input = document.createElement('input');
        input.value = url;
        document.body.appendChild(input);
        input.select();
        try {
            document.execCommand('copy');
            showMessage('success', '✅ 已复制: ' + url);
        } catch(e) {
            showMessage('error', '❌ 复制失败: ' + url);
        }
        document.body.removeChild(input);
    }

    // 加载固件列表
    async function loadFiles() {
        try {
            const r = await fetch('/api/list');
            const data = await r.json();
            const div = document.getElementById('firmwareList');
            const files = data.files || [];
            if (files.length === 0) {
                div.innerHTML = '<div class="alert alert-info">暂无固件文件</div>';
                return;
            }
            div.innerHTML = files.map(f => {
                const kb = (f.size_bytes / 1024).toFixed(2);
                const timeStr = f.time ? new Date(f.time * 1000).toLocaleString('zh-CN') : '-';
                const firmwareType = getFirmwareType(f.name);
                let typeLabel = '';
                if (firmwareType === 'system') typeLabel = '<span style="background: #10b981; color: white; padding: 2px 8px; border-radius: 4px; font-size: 0.8em; margin-left: 8px;">系统固件</span>';
                else if (firmwareType === 'web') typeLabel = '<span style="background: #667eea; color: white; padding: 2px 8px; border-radius: 4px; font-size: 0.8em; margin-left: 8px;">网页固件</span>';
                const isSelected = selectedFiles.has(f.name);
                return `<div class="firmware-item">
                    <div style="margin-right: 12px;">
                        <input type="checkbox" class="firmware-checkbox" value="${esc(f.name)}" ${isSelected ? 'checked' : ''} onchange="toggleFileSelect('${esc(f.name)}', this)">
                    </div>
                    <div class="firmware-info">
                        <div class="firmware-name">📦 ${esc(f.name)}${typeLabel}</div>
                        <div class="firmware-meta">大小: ${kb} KB | 上传时间: ${timeStr}</div>
                    </div>
                    <div class="firmware-actions">
                        <button class="btn btn-success btn-small" onclick="copyUrl('${esc(f.name)}')">📋 复制URL</button>
                        <button class="btn btn-danger btn-small" onclick="deleteFirmware('${esc(f.name)}')">🗑️ 删除</button>
                    </div>
                </div>`;
            }).join('');
            // 检查是否全选
            const checkboxes = document.querySelectorAll('.firmware-checkbox');
            const allChecked = checkboxes.length > 0 && Array.from(checkboxes).every(cb => selectedFiles.has(cb.value));
            document.getElementById('selectAll').checked = allChecked;
        } catch(e) {
            document.getElementById('firmwareList').innerHTML = '<div class="alert alert-error">加载失败</div>';
        }
    }

    function esc(s) { return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;'); }

    // 拖拽上传
    const uploadArea = document.getElementById('uploadArea');
    uploadArea.addEventListener('dragover', (e) => { e.preventDefault(); uploadArea.classList.add('dragover'); });
    uploadArea.addEventListener('dragleave', () => { uploadArea.classList.remove('dragover'); });
    uploadArea.addEventListener('drop', (e) => {
        e.preventDefault();
        uploadArea.classList.remove('dragover');
        const file = e.dataTransfer.files[0];
        if (file) uploadFirmware(file);
    });
    </script>
</body>
</html>"""


# ═══════════════════════════════════════════════════════════
# HTTP 请求处理器
# ═══════════════════════════════════════════════════════════
class OTAHandler(http.server.BaseHTTPRequestHandler):

    def log_message(self, format, *args):
        ts = time.strftime("%H:%M:%S")
        print(f"[{ts}][{self.client_address[0]}] {args[0]}")

    def send_json(self, data, status=200):
        body = json.dumps(data, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", len(body))
        self.end_headers()
        self.wfile.write(body)

    def send_html(self, html, status=200):
        body = html.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", len(body))
        self.end_headers()
        self.wfile.write(body)

    def serve_file(self, filepath):
        try:
            with open(filepath, "rb") as f:
                data = f.read()
        except FileNotFoundError:
            self.send_json({"error": "file not found"}, 404)
            return
        ext = os.path.splitext(filepath)[1].lower()
        mime_map = {
            ".bin": "application/octet-stream",
            ".json": "application/json",
            ".html": "text/html",
            ".css": "text/css",
            ".js": "application/javascript",
        }
        self.send_response(200)
        self.send_header("Content-Type", mime_map.get(ext, "application/octet-stream"))
        self.send_header("Content-Length", len(data))
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/" or path == "":
            self.send_html(MANAGEMENT_HTML)
        elif path.startswith("/firmware/"):
            filename = path[len("/firmware/"):]
            if ".." in filename or "/" in filename or "\\" in filename:
                self.send_json({"error": "invalid filename"}, 400)
                return
            self.serve_file(str(FIRMWARE_DIR / filename))
        elif path == "/api/list":
            self.handle_list()
        else:
            self.send_json({"error": "not found"}, 404)

    def do_POST(self):
        path = self.path.split("?")[0]
        if path == "/api/upload":
            self.handle_upload()
        elif path == "/api/save_config":
            self.handle_save_config()
        elif path == "/api/delete":
            self.handle_delete()
        elif path == "/api/delete_batch":
            self.handle_delete_batch()
        elif path == "/api/select_folder":
            self.handle_select_folder()
        elif path == "/api/list":
            self.handle_list()
        else:
            self.send_json({"error": "not found"}, 404)

    def handle_upload(self):
        content_type = self.headers.get("Content-Type", "")
        if "multipart/form-data" not in content_type:
            self.send_json({"success": False, "message": "需要 multipart/form-data"}, 400)
            return

        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length)
        boundary = content_type.split("boundary=")[-1].encode()

        for part in body.split(b"--" + boundary):
            if b"filename=\"" in part:
                header_end = part.find(b"\r\n\r\n")
                if header_end == -1:
                    continue
                header = part[:header_end].decode("utf-8", errors="ignore")
                file_data = part[header_end + 4:]
                if file_data.endswith(b"\r\n"):
                    file_data = file_data[:-2]

                match = re.search(r'filename="([^"]*)"', header)
                if not match:
                    continue
                filename = os.path.basename(match.group(1))

                if not filename.lower().endswith(".bin"):
                    self.send_json({"success": False, "message": "只支持 .bin 格式的固件文件"}, 400)
                    return

                # 安全检查：文件名只保留安全字符
                safe_name = re.sub(r'[^a-zA-Z0-9._-]', '_', filename)
                if safe_name != filename:
                    filename = safe_name

                filepath = FIRMWARE_DIR / filename
                # 重名则加时间戳
                if filepath.exists():
                    name_part = os.path.splitext(filename)[0]
                    filename = f"{name_part}_{int(time.time())}.bin"
                    filepath = FIRMWARE_DIR / filename

                with open(filepath, "wb") as f:
                    f.write(file_data)

                fsize = filepath.stat().st_size
                print(f"  ✓ 上传: {filename} ({fsize} bytes)")
                self.send_json({
                    "success": True,
                    "message": "上传成功",
                    "filename": filename,
                    "size": fsize,
                })
                return

        self.send_json({"success": False, "message": "未找到文件"}, 400)

    def handle_save_config(self):
        content_length = int(self.headers.get("Content-Length", 0))
        try:
            data = json.loads(self.rfile.read(content_length).decode("utf-8"))
        except json.JSONDecodeError:
            self.send_json({"success": False, "message": "无效的JSON数据"}, 400)
            return

        if not data.get("version"):
            self.send_json({"success": False, "message": "版本号不能为空"}, 400)
            return

        has_url = bool(data.get("firmware_url")) and data.get("firmware_enabled", True)
        has_spiffs = bool(data.get("spiffs_url")) and data.get("spiffs_enabled", True)

        if not has_url and not has_spiffs:
            self.send_json({"success": False, "message": "请至少勾选一项更新内容且对应URL不能为空"}, 400)
            return

        # 如果勾选了更新但 URL 为空，自动禁用该项，避免设备端用空 URL 执行本地拷贝
        firmware_enabled = bool(data.get("firmware_enabled", True)) and bool(data.get("firmware_url"))
        spiffs_enabled = bool(data.get("spiffs_enabled", True)) and bool(data.get("spiffs_url"))

        config = {
            "version": str(data["version"]),
            "firmware_url": str(data.get("firmware_url", "")),
            "spiffs_url": str(data.get("spiffs_url", "")),
            "description": str(data.get("description", "")),
            "force": bool(data.get("force", False)),
            "firmware_enabled": firmware_enabled,
            "spiffs_enabled": spiffs_enabled,
        }

        with open(FIRMWARE_DIR / "version.json", "w", encoding="utf-8") as f:
            json.dump(config, f, ensure_ascii=False, indent=4)

        print(f"  ✓ 配置已保存: version={config['version']}")
        self.send_json({"success": True, "message": "配置保存成功"})

    def handle_delete(self):
        content_length = int(self.headers.get("Content-Length", 0))
        try:
            data = json.loads(self.rfile.read(content_length).decode("utf-8"))
        except json.JSONDecodeError:
            self.send_json({"success": False, "message": "无效的JSON数据"}, 400)
            return

        filename = data.get("filename", "")
        if not filename:
            self.send_json({"success": False, "message": "文件名不能为空"}, 400)
            return

        safe_name = os.path.basename(filename)
        if safe_name != filename or ".." in filename or "/" in filename or "\\" in filename:
            self.send_json({"success": False, "message": "无效的文件名"}, 400)
            return

        if safe_name == "version.json":
            self.send_json({"success": False, "message": "不能删除配置文件"}, 400)
            return

        filepath = FIRMWARE_DIR / safe_name
        try:
            os.remove(filepath)
            print(f"  ✓ 删除: {safe_name}")
            self.send_json({"success": True, "message": "删除成功"})
        except FileNotFoundError:
            self.send_json({"success": False, "message": "文件不存在"}, 404)

    def handle_delete_batch(self):
        content_length = int(self.headers.get("Content-Length", 0))
        try:
            data = json.loads(self.rfile.read(content_length).decode("utf-8"))
        except json.JSONDecodeError:
            self.send_json({"success": False, "message": "无效的JSON数据"}, 400)
            return

        filenames = data.get("filenames", [])
        if not filenames:
            self.send_json({"success": False, "message": "请选择要删除的文件"}, 400)
            return

        deleted_count = 0
        for filename in filenames:
            safe_name = os.path.basename(filename)
            if safe_name != filename or ".." in filename or "/" in filename or "\\" in filename:
                continue
            if safe_name == "version.json":
                continue
            filepath = FIRMWARE_DIR / safe_name
            try:
                os.remove(filepath)
                print(f"  ✓ 删除: {safe_name}")
                deleted_count += 1
            except FileNotFoundError:
                continue

        self.send_json({
            "success": True,
            "message": f"成功删除 {deleted_count} 个文件",
            "deleted_count": deleted_count
        })

    def handle_select_folder(self):
        global _last_selected_folder
        import tkinter as tk
        from tkinter import filedialog

        # 读取前端传来的 checkbox 状态
        content_len = int(self.headers.get('Content-Length', 0))
        body = {}
        if content_len > 0:
            try:
                body = json.loads(self.rfile.read(content_len))
            except Exception:
                pass
        firmware_enabled = body.get("firmware_enabled", True)
        spiffs_enabled = body.get("spiffs_enabled", True)

        # 创建隐藏的 Tkinter 窗口
        root = tk.Tk()
        root.withdraw()
        root.wm_attributes('-topmost', True)
        
        # 从上一次选择的路径开始
        initial = _last_selected_folder or str(Path.home())
        folder_path = filedialog.askdirectory(
            title="选择固件文件夹",
            initialdir=initial
        )
        root.destroy()
        
        if not folder_path:
            self.send_json({"success": False, "message": "未选择文件夹"})
            return

        _last_selected_folder = folder_path
        _save_config()
        system_fw, web_fw = scan_folder_for_firmware(folder_path)
        if not system_fw and not web_fw:
            self.send_json({"success": False, "message": "未找到 SuperDongle.bin 或 spiffs 相关固件文件"}, 400)
            return

        result = {"success": True, "system_firmware": None, "web_firmware": None}
        if system_fw:
            result["system_firmware"] = copy_firmware_to_storage(system_fw)
        if web_fw:
            result["web_firmware"] = copy_firmware_to_storage(web_fw)

        # 自动保存配置到 version.json，尊重前端的 checkbox 状态
        config_path = FIRMWARE_DIR / "version.json"
        config = {}
        if config_path.exists():
            try:
                with open(config_path, "r", encoding="utf-8") as f:
                    config = json.load(f)
            except Exception:
                pass
        config["force"] = config.get("force", False)
        config["description"] = config.get("description", "")
        config["version"] = config.get("version", "")
        origin = f"http://{self.headers.get('Host', 'localhost')}"
        if result["system_firmware"]:
            config["firmware_url"] = f"{origin}/firmware/{result['system_firmware']}"
            config["firmware_enabled"] = firmware_enabled
        if result["web_firmware"]:
            config["spiffs_url"] = f"{origin}/firmware/{result['web_firmware']}"
            config["spiffs_enabled"] = spiffs_enabled
        # URL 为空时自动禁用对应项
        if not config.get("firmware_url"):
            config["firmware_enabled"] = False
        if not config.get("spiffs_url"):
            config["spiffs_enabled"] = False
        with open(config_path, "w", encoding="utf-8") as f:
            json.dump(config, f, ensure_ascii=False, indent=4)
        print(f"  ✓ 配置已自动保存: version={config.get('version', '')} fw={config.get('firmware_enabled')} spiffs={config.get('spiffs_enabled')}")

        self.send_json(result)

    def handle_list(self):
        files = []
        for f in sorted(FIRMWARE_DIR.iterdir()):
            if f.is_file() and f.name != "version.json":
                files.append({
                    "name": f.name,
                    "size_bytes": f.stat().st_size,
                    "time": int(f.stat().st_mtime),
                })
        self.send_json({"files": files})


# ═══════════════════════════════════════════════════════════
# 启动
# ═══════════════════════════════════════════════════════════
def get_local_ips():
    ips = []
    # 枚举所有网卡获取更全面
    try:
        if os.name == 'nt':
            # Windows 用 ipconfig
            result = subprocess.run(['ipconfig'], capture_output=True, text=True, encoding='gbk', errors='ignore')
            if result.returncode == 0:
                # 匹配所有 IPv4 地址（兼容中英文）
                ipv4_pattern = r'(?:IPv4 地址|IPv4 Address)[ .]*: ([\d.]+)'
                matches = re.findall(ipv4_pattern, result.stdout)
                for ip in matches:
                    if ip and not ip.startswith('127.') and not ip.startswith('169.254.'):
                        if ip not in ips:
                            ips.append(ip)
        else:
            # Linux/macOS 用 ifconfig 或 ip addr
            try:
                result = subprocess.run(['ip', 'addr'], capture_output=True, text=True)
                if result.returncode == 0:
                    ipv4_pattern = r'inet ([\d.]+)/\d+'
                    matches = re.findall(ipv4_pattern, result.stdout)
                    for ip in matches:
                        if ip and not ip.startswith('127.') and not ip.startswith('169.254.'):
                            if ip not in ips:
                                ips.append(ip)
            except FileNotFoundError:
                result = subprocess.run(['ifconfig'], capture_output=True, text=True)
                if result.returncode == 0:
                    ipv4_pattern = r'inet ([\d.]+)'
                    matches = re.findall(ipv4_pattern, result.stdout)
                    for ip in matches:
                        if ip and not ip.startswith('127.') and not ip.startswith('169.254.'):
                            if ip not in ips:
                                ips.append(ip)
    except Exception:
        pass
    # 如果上面没拿到，兜底用原来的方法
    if not ips:
        try:
            hostname = socket.gethostname()
            ip = socket.gethostbyname(hostname)
            if not ip.startswith("127.") and not ip.startswith("169.254."):
                ips.append(ip)
        except Exception:
            pass
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            ip = s.getsockname()[0]
            s.close()
            if ip not in ips and not ip.startswith("127.") and not ip.startswith("169.254."):
                ips.append(ip)
        except Exception:
            pass
    # 排序：优先私有地址（192.168.x.x, 10.x.x.x, 172.16-31.x.x
    def ip_priority(ip):
        if ip.startswith('192.168.'):
            return 0
        if ip.startswith('10.'):
            return 1
        if ip.startswith('172.') and 16 <= int(ip.split('.')[1]) <= 31:
            return 2
        return 3
    ips = sorted(ips, key=ip_priority)
    return ips


def setup_firewall_rule(port):
    if os.name != 'nt':
        return False
    rule_name = f"OTA Server {port}"
    try:
        # 检查规则是否已存在（不读取输出，避免编码问题）
        result = subprocess.run(
            ['netsh', 'advfirewall', 'firewall', 'show', 'rule', 'name=' + rule_name],
            capture_output=True, timeout=5
        )
        if result.returncode == 0:
            print(f"  [防火墙] 规则 '{rule_name}' 已存在")
            return True
        # 添加规则
        result = subprocess.run(
            ['netsh', 'advfirewall', 'firewall', 'add', 'rule', 
             'name=' + rule_name, 'dir=in', 'action=allow', 
             f'protocol=TCP', f'localport={port}'],
            capture_output=True, timeout=10
        )
        if result.returncode == 0:
            print(f"  [防火墙] 已添加规则：允许 TCP 端口 {port} 入站")
            return True
        else:
            print(f"  [防火墙] 添加规则失败，需要管理员权限")
            print(f"  [防火墙] 请手动运行：netsh advfirewall firewall add rule name=\"{rule_name}\" dir=in action=allow protocol=TCP localport={port}")
            return False
    except Exception as e:
        print(f"  [防火墙] 配置失败：{e}")
        return False

def is_admin():
    if os.name != 'nt':
        return False
    try:
        return ctypes.windll.shell32.IsUserAnAdmin() != 0
    except:
        return False

def main():
    _load_config()
    parser = argparse.ArgumentParser(description="本地 OTA 固件服务器")
    parser.add_argument("--port", type=int, default=8000, help="监听端口 (默认: 8000)")
    parser.add_argument("--host", type=str, default="0.0.0.0", help="监听地址 (默认: 0.0.0.0)")
    parser.add_argument("--no-open", action="store_true", help="不自动打开浏览器")
    args = parser.parse_args()

    # 直接简单检查规则是否存在（不需要管理员权限）
    rule_name = f"OTA Server {args.port}"
    rule_exists = False
    print(f"检查防火墙规则 {rule_name} 是否存在...")
    try:
        result = subprocess.run(
            f'netsh advfirewall firewall show rule name="{rule_name}"',
            capture_output=True,
            shell=True,
            timeout=3
        )
        rule_exists = (result.returncode == 0)
        print(f"规则检查结果：{'已存在' if rule_exists else '不存在'}")
    except Exception as e:
        print(f"检查规则时出错：{e}，假设规则不存在")
    
    if not rule_exists:
        # 检查是否是管理员
        if not is_admin():
            print("需要管理员权限来配置防火墙，正在尝试请求权限...")
            try:
                # 重新启动脚本并请求管理员权限
                script_path = os.path.abspath(sys.argv[0])
                params = f'--port {args.port} --host {args.host}'
                ctypes.windll.shell32.ShellExecuteW(None, "runas", sys.executable, f'"{script_path}" {params}', None, 1)
                sys.exit(0)
            except Exception as e:
                print(f"请求管理员权限失败：{e}")
                print("请手动以管理员身份运行此脚本，或使用8080端口")
        
        # 尝试设置防火墙规则
        print("正在配置防火墙...")
        setup_firewall_rule(args.port)

    server = http.server.HTTPServer((args.host, args.port), OTAHandler)

    print("=" * 56)
    print("  OTA 固件管理服务器")
    print("=" * 56)
    print(f"  固件目录: {FIRMWARE_DIR}")
    print()
    ips = get_local_ips()
    if ips:
        print("  局域网地址:")
        for ip in ips:
            print(f"    - 管理页面: http://{ip}:{args.port}")
            print(f"    - version.json: http://{ip}:{args.port}/firmware/version.json")
    else:
        print("  (未能检测到局域网 IP，请手动查看)")
    print()
    if ips:
        print(f"  ESP32 version_url → http://{ips[0]}:{args.port}/firmware/version.json")
    print()
    print("  Ctrl+C 停止")
    print("=" * 56)

    # 自动打开浏览器
    if not getattr(args, 'no_open', False) and ips:
        url = f"http://{ips[0]}:{args.port}"
        print(f"  打开浏览器: {url}")
        try:
            # Windows 上用 cmd /c start 更可靠
            if os.name == 'nt':
                subprocess.Popen(['cmd', '/c', 'start', url], shell=True)
            else:
                webbrowser.open(url)
        except Exception as e:
            print(f"  打开浏览器失败: {e}")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n服务器已停止")
        server.server_close()


if __name__ == "__main__":
    main()
