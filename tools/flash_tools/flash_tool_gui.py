#!/usr/bin/env python3
"""
ESP32-S3 Flash 工具 - 图形界面
===============================
提取固件 + 验证 Flash 加密状态。

直接运行：
    python flash_tool_gui.py

依赖：esptool.py（IDF 环境自带）
"""

import math
import os
import shlex
import struct
import subprocess
import sys
import threading
import time
import tkinter as tk
from collections import Counter
from tkinter import ttk, scrolledtext, filedialog, messagebox


# ==================== esptool 发现 ====================

def _find_esptool():
    """
    查找 esptool.py 的完整路径。
    优先用 pip 安装的，其次用 IDF 环境下的。
    """
    # 1. 先用 pip show 找安装路径
    try:
        result = subprocess.run(
            [sys.executable, "-m", "pip", "show", "esptool"],
            capture_output=True, text=True, timeout=10
        )
        if result.returncode == 0:
            for line in result.stdout.splitlines():
                if line.startswith("Location:"):
                    loc = line.split(":", 1)[1].strip()
                    candidate = os.path.join(loc, "esptool", "__main__.py")
                    if os.path.exists(candidate):
                        return [sys.executable, candidate]
                    candidate = os.path.join(loc, "esptool.py")
                    if os.path.exists(candidate):
                        return [sys.executable, candidate]
    except Exception:
        pass

    # 2. 试 IDF 环境常见路径
    idf_venvs = [
        r"C:\Espressif\tools\python\v5.5.4\venv\Scripts",
        r"C:\Espressif\tools\python\v5.4\venv\Scripts",
        r"C:\Espressif\python_env\idf5.5_py3.11_env\Scripts",
    ]
    for venv in idf_venvs:
        esptool_exe = os.path.join(venv, "esptool.py.exe")
        if os.path.exists(esptool_exe):
            return [sys.executable, esptool_exe]
        esptool_py = os.path.join(venv, "esptool.py")
        if os.path.exists(esptool_py):
            return [sys.executable, esptool_py]

    # 3. 直接用 python -m esptool
    try:
        result = subprocess.run(
            [sys.executable, "-m", "esptool", "version"],
            capture_output=True, text=True, timeout=10
        )
        if result.returncode == 0:
            return [sys.executable, "-m", "esptool"]
    except Exception:
        pass

    # 4. 回退到全局 PATH
    try:
        result = subprocess.run(
            ["esptool.py", "version"],
            capture_output=True, text=True, timeout=10
        )
        if result.returncode == 0:
            # 找完整路径
            r = subprocess.run(
                ["where", "esptool.py"],
                capture_output=True, text=True, timeout=5
            )
            if r.returncode == 0 and r.stdout.strip():
                return [r.stdout.strip().splitlines()[0]]
            return ["esptool.py"]
    except Exception:
        pass

    return None


_ESPTool_CMD = None


def _get_esptool_cmd():
    """延迟初始化 esptool 命令"""
    global _ESPTool_CMD
    if _ESPTool_CMD is None:
        _ESPTool_CMD = _find_esptool()
    return _ESPTool_CMD


# ==================== 端口扫描 ====================

def get_available_ports():
    ports = []
    try:
        from serial.tools.list_ports import comports
        for p in comports():
            ports.append(p.device)
    except ImportError:
        pass

    if not ports:
        for i in range(1, 33):
            try:
                import serial
                s = serial.Serial(f"COM{i}", timeout=0.1)
                s.close()
                ports.append(f"COM{i}")
            except Exception:
                pass
    return ports


# ==================== 工具函数 ====================

def run_esptool(extra_args, timeout=120):
    cmd = _get_esptool_cmd()
    if cmd is None:
        raise RuntimeError("找不到 esptool.py。请确保 ESP-IDF 已安装或 pip install esptool")

    full_cmd = cmd + extra_args
    print(f"[CMD] {shlex.join(full_cmd)}")

    result = subprocess.run(
        full_cmd,
        capture_output=True,
        text=True,
        timeout=timeout
    )

    stdout = result.stdout.strip()
    stderr = result.stderr.strip()

    if result.returncode != 0:
        err = stderr or stdout
        raise RuntimeError(f"esptool 失败:\n{err}")

    return stdout, stderr


def calculate_entropy(data):
    if not data:
        return 0
    total = len(data)
    counter = Counter(data)
    entropy = 0.0
    for count in counter.values():
        p = count / total
        entropy -= p * math.log2(p)
    return entropy


def is_likely_encrypted(filepath):
    with open(filepath, "rb") as f:
        data = f.read()

    if len(data) < 64:
        return False, 0, "文件太小，无法判断"

    entropy = calculate_entropy(data)

    has_elf_header = data[:4] == b"\x7fELF"
    has_esp_image = data[:4] == b"\xe9\x00" or data[:4] == b"\xe9\xff"
    has_text = sum(1 for b in data[:1024] if 0x20 <= b <= 0x7E) > 500

    reasons = []
    if has_elf_header:
        reasons.append("ELF 文件头")
    if has_esp_image:
        reasons.append("ESP 镜像魔数")
    if has_text:
        reasons.append("大量可读文本")

    if entropy > 7.8 and not reasons:
        return True, entropy, "熵值极高，无明文特征 → 已加密"
    elif entropy > 7.5:
        return True, entropy, "熵值较高 → 很可能是密文"
    elif entropy < 4:
        return False, entropy, f"熵值很低({entropy:.2f}) → 明文"
    else:
        detail = ", ".join(reasons) if reasons else f"熵值中等({entropy:.2f})"
        return False, entropy, f"检测到明文特征: {detail}"


# ==================== GUI ====================

class FlashToolGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("ESP32-S3 Flash 工具")
        self.root.geometry("720x560")
        self.root.resizable(True, True)

        style = ttk.Style()
        style.theme_use("clam")

        self._build_ui()
        self._refresh_ports()

    def _build_ui(self):
        # ===== 顶部 =====
        top = ttk.Frame(self.root, padding="10 10 10 5")
        top.pack(fill=tk.X)

        ttk.Label(top, text="COM 端口:").pack(side=tk.LEFT)
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(top, textvariable=self.port_var, width=12, state="readonly")
        self.port_combo.pack(side=tk.LEFT, padx=5)
        ttk.Button(top, text="刷新", width=5, command=self._refresh_ports).pack(side=tk.LEFT, padx=2)
        ttk.Label(top, text="请连接设备后点击「刷新」", foreground="gray").pack(side=tk.LEFT, padx=10)

        # ===== Notebook =====
        nb = ttk.Notebook(self.root, padding="10 5 10 10")
        nb.pack(fill=tk.BOTH, expand=True)

        self.tab_read = ttk.Frame(nb, padding="10")
        nb.add(self.tab_read, text=" 提取固件 ")
        self._build_read_tab()

        self.tab_verify = ttk.Frame(nb, padding="10")
        nb.add(self.tab_verify, text=" 验证加密 ")
        self._build_verify_tab()

        self.tab_spiffs = ttk.Frame(nb, padding="10")
        nb.add(self.tab_spiffs, text=" 解包 SPIFFS ")
        self._build_spiffs_tab()

        self.tab_ptable = ttk.Frame(nb, padding="10")
        nb.add(self.tab_ptable, text=" 解析分区表 ")
        self._build_ptable_tab()

        # ===== 底部状态栏 =====
        sf = ttk.Frame(self.root, padding="10 5 10 10")
        sf.pack(fill=tk.X, side=tk.BOTTOM)
        self.status = ttk.Label(sf, text="就绪", relief=tk.SUNKEN, anchor=tk.W)
        self.status.pack(fill=tk.X)
        self.progress = ttk.Progressbar(sf, mode="indeterminate", length=150)

    # ---------- Tab 1: 提取固件 ----------
    def _build_read_tab(self):
        f = ttk.Frame(self.tab_read)
        f.pack(fill=tk.X, pady=(0, 10))

        ttk.Label(f, text="起始地址:").grid(row=0, column=0, sticky=tk.W, pady=2)
        self.addr = ttk.Entry(f, width=18)
        self.addr.insert(0, "0x0")
        self.addr.grid(row=0, column=1, padx=5, pady=2)

        ttk.Label(f, text="大小:").grid(row=0, column=2, sticky=tk.W, pady=2, padx=(20, 0))
        self.size = ttk.Entry(f, width=18)
        self.size.insert(0, "ALL")
        self.size.grid(row=0, column=3, padx=5, pady=2)

        ttk.Label(f, text="保存为:").grid(row=1, column=0, sticky=tk.W, pady=2)
        self.out = ttk.Entry(f, width=42)
        self.out.insert(0, "full_flash_dump.bin")
        self.out.grid(row=1, column=1, columnspan=2, padx=5, pady=2)
        ttk.Button(f, text="浏览...", width=7, command=self._browse).grid(row=1, column=3, padx=5, pady=2)

        # 快捷预设
        pf = ttk.Frame(self.tab_read)
        pf.pack(fill=tk.X, pady=(0, 8))
        ttk.Label(pf, text="快捷预设:").pack(side=tk.LEFT)
        presets = [
            ("全片 ALL",        "0x0",      "ALL"),
            ("ota_0 2.5M分区",  "0x1A000",  "0x271000"),
            ("ota_0 实际~1.3M", "0x1A000",  "0x13A000"),
            ("ota_1 2.5M分区",  "0x28B000", "0x271000"),
            ("SPIFFS_0 1M",     "0x4FC000", "0xFA000"),
            ("bootloader 64K",  "0x0",      "0x10000"),
        ]
        for name, a, s in presets:
            ttk.Button(pf, text=name, command=lambda aa=a, ss=s: self._preset(aa, ss)).pack(side=tk.LEFT, padx=2)

        ttk.Button(self.tab_read, text="开始提取", width=16, command=self._do_read).pack(pady=8)

        ttk.Label(self.tab_read, text="输出:").pack(anchor=tk.W)
        self.rlog = scrolledtext.ScrolledText(self.tab_read, height=12, state=tk.DISABLED, font=("Consolas", 9))
        self.rlog.pack(fill=tk.BOTH, expand=True)

    # ---------- Tab 2: 验证加密 ----------
    def _build_verify_tab(self):
        f = ttk.Frame(self.tab_verify)
        f.pack(fill=tk.X, pady=(0, 10))

        ttk.Label(f, text="检测地址:").grid(row=0, column=0, sticky=tk.W, pady=2)
        self.va = ttk.Entry(f, width=18)
        self.va.insert(0, "0x1A000")
        self.va.grid(row=0, column=1, padx=5, pady=2)

        ttk.Label(f, text="大小:").grid(row=0, column=2, sticky=tk.W, pady=2, padx=(20, 0))
        self.vs = ttk.Entry(f, width=18)
        self.vs.insert(0, "0x1000")
        self.vs.grid(row=0, column=3, padx=5, pady=2)

        ttk.Label(
            self.tab_verify,
            text="原理：读取一段 Flash → 计算香农熵 → 熵值 > 7.5 且无 ELF/ESP 明文特征 = 已加密",
            foreground="gray"
        ).pack(anchor=tk.W, pady=(0, 5))

        ttk.Button(self.tab_verify, text="验证加密状态", width=16, command=self._do_verify).pack(pady=5)

        self.vframe = ttk.LabelFrame(self.tab_verify, text="验证结果", padding="10")
        self.vframe.pack(fill=tk.X, pady=(10, 5))
        self.vresult = tk.StringVar(value="等待检测...")
        ttk.Label(self.vframe, textvariable=self.vresult, font=("Microsoft YaHei UI", 12)).pack()

        ttk.Label(self.tab_verify, text="输出:").pack(anchor=tk.W)
        self.vlog = scrolledtext.ScrolledText(self.tab_verify, height=10, state=tk.DISABLED, font=("Consolas", 9))
        self.vlog.pack(fill=tk.BOTH, expand=True)

    # ==================== 解析分区表 ====================

    def _build_ptable_tab(self):
        f1 = ttk.Frame(self.tab_ptable)
        f1.pack(fill=tk.X, pady=(0, 10))

        ttk.Label(f1, text="全片 dump:").grid(row=0, column=0, sticky=tk.W, pady=2)
        self.ptable_file = ttk.Entry(f1, width=42)
        self.ptable_file.grid(row=0, column=1, columnspan=2, padx=5, pady=2)
        ttk.Button(f1, text="浏览...", width=7, command=self._browse_ptable).grid(row=0, column=3, padx=5, pady=2)

        ttk.Label(f1, text="分区表偏移:").grid(row=1, column=0, sticky=tk.W, pady=2)
        self.ptable_offset = ttk.Entry(f1, width=18)
        self.ptable_offset.insert(0, "0x10000")
        self.ptable_offset.grid(row=1, column=1, padx=5, pady=2)

        ttk.Label(f1, text="（默认 0x10000 = 64KB）", foreground="gray").grid(
            row=1, column=2, columnspan=2, sticky=tk.W)

        ttk.Button(self.tab_ptable, text="解析", width=16, command=self._do_parse_ptable).pack(pady=8)

        ttk.Label(self.tab_ptable, text="结果:").pack(anchor=tk.W)
        self.plog = scrolledtext.ScrolledText(self.tab_ptable, height=20, state=tk.DISABLED, font=("Consolas", 9))
        self.plog.pack(fill=tk.BOTH, expand=True)

    def _browse_ptable(self):
        p = filedialog.askopenfilename(filetypes=[("Bin", "*.bin"), ("All", "*.*")])
        if p:
            self.ptable_file.delete(0, tk.END)
            self.ptable_file.insert(0, p)

    def _do_parse_ptable(self):
        dump = self.ptable_file.get().strip()
        w = self.plog

        if not dump or not os.path.exists(dump):
            messagebox.showwarning("文件不存在", f"找不到 dump 文件: {dump}")
            return

        off_str = self.ptable_offset.get().strip()
        ptoff = int(off_str, 16) if off_str.lower().startswith("0x") else int(off_str)

        TYPE_NAMES = {0: "app", 1: "data"}
        SUBTYPE_NAMES = {
            0: {0: "factory", 16: "ota_0", 17: "ota_1", 18: "ota_2", 19: "ota_3",
                20: "ota_4", 21: "ota_5", 22: "ota_6", 23: "ota_7",
                24: "ota_8", 25: "ota_9", 26: "ota_10", 27: "ota_11",
                28: "ota_12", 29: "ota_13", 30: "ota_14", 31: "ota_15",
                32: "test"},
            1: {0: "ota", 1: "phy", 2: "nvs", 3: "coredump",
                4: "nvs_keys", 5: "efuse", 7: "esphttpd",
                8: "fat", 9: "spiffs", 10: "littlefs"},
        }

        self._log(w, f"{'='*60}")
        self._log(w, f"解析分区表: {dump}")
        self._log(w, f"分区表偏移: 0x{ptoff:X}")

        try:
            with open(dump, "rb") as f:
                f.seek(ptoff)
                raw = f.read(0xC00)  # 最多 3072 bytes (96 entries × 32B)

            found = []
            for i in range(0, len(raw), 32):
                entry = raw[i:i+32]
                if len(entry) < 32:
                    break
                magic, ptype, subtype = struct.unpack("<HBB", entry[:4])
                if magic != 0xAA50:
                    continue
                offset, size = struct.unpack("<II", entry[4:12])
                label_bytes = entry[12:28]
                label = label_bytes.decode("ascii", errors="replace").rstrip("\x00")
                flags = struct.unpack("<I", entry[28:32])[0]

                tname = TYPE_NAMES.get(ptype, f"type_{ptype}")
                sname = SUBTYPE_NAMES.get(ptype, {}).get(subtype, f"sub_{subtype}")

                found.append((offset, size, tname, sname, label, flags, ptoff + offset))

            if not found:
                self._log(w, f"\n✗ 未找到有效分区项")
                self._log(w, f"  偏移 0x{ptoff:X} 处可能是密文（Flash Encryption 已启用）")
                self._log(w, f"  提示：加密后 dump 无法解析分区表")
                return

            # 输出
            header = f"\n{'地址':<12} {'大小':<12} {'名称':<16} {'类型':<10} {'子类型':<10} 说明"
            self._log(w, header)
            self._log(w, "-" * 90)

            for offset, size, tname, sname, label, flags, abs_addr in found:
                size_k = size // 1024
                size_str = f"{size_k}K" if size_k < 1024 else f"{size_k // 1024:.1f}M"
                note = ""
                if label == "spiffs_0" or label == "spiffs_1":
                    note = "← 网页文件"
                elif label.startswith("ota_"):
                    note = "← 固件"
                elif label == "nvs":
                    note = "← 设备参数"
                self._log(w, f"0x{abs_addr:08X}  {size_str:<11} {label:<16} {tname:<10} {sname:<10} {note}")

            self._log(w, f"\n共 {len(found)} 个分区")

        except Exception as e:
            self._log(w, f"✗ 解析失败：{e}")

    # ==================== 辅助 ====================

    def _log(self, w, msg):
        w.config(state=tk.NORMAL)
        w.insert(tk.END, msg + "\n")
        w.see(tk.END)
        w.config(state=tk.DISABLED)

    def _status(self, msg):
        self.status.config(text=msg)

    def _port(self):
        p = self.port_var.get()
        if not p:
            messagebox.showwarning("未选择端口", "请先选择 COM 端口后重试。")
        return p

    def _refresh_ports(self):
        self._status("扫描 COM 端口...")
        ports = get_available_ports()
        self.port_combo["values"] = ports
        if ports:
            self.port_combo.current(0)
            self._status(f"检测到 {len(ports)} 个端口")
        else:
            self._status("未检测到端口")
            messagebox.showwarning("无设备", "未检测到 COM 端口。\n请连接 ESP32-S3 后点击「刷新」。")

    def _browse(self):
        p = filedialog.asksaveasfilename(defaultextension=".bin", filetypes=[("Bin", "*.bin"), ("All", "*.*")])
        if p:
            self.out.delete(0, tk.END)
            self.out.insert(0, p)

    def _preset(self, addr, size):
        self.addr.delete(0, tk.END)
        self.addr.insert(0, addr)
        self.size.delete(0, tk.END)
        self.size.insert(0, size)

    # ==================== 提取 ====================

    CHUNK_SIZE = 0x20000  # 128KB

    def _do_read(self):
        port = self._port()
        if not port:
            return
        addr_str = self.addr.get().strip()
        size_str = self.size.get().strip()
        out = self.out.get().strip() or "dump.bin"
        w = self.rlog

        is_all = size_str.upper() == "ALL"
        flash_size = 0x1000000  # 16MB

        if is_all:
            addr = 0
            size = flash_size
        else:
            addr = int(addr_str, 16) if addr_str.lower().startswith("0x") else int(addr_str)
            size = int(size_str, 16) if size_str.lower().startswith("0x") else int(size_str)

        total_chunks = (size + self.CHUNK_SIZE - 1) // self.CHUNK_SIZE

        self._log(w, f"{'='*50}")
        self._log(w, f"提取: {port}  {'全片 ALL' if is_all else f'addr={addr_str} size={size_str}'} "
                    f"({size} bytes) → {out}")
        self._log(w, f"分块: {total_chunks} 块 × {self.CHUNK_SIZE // 1024}KB")

        def task():
            try:
                # 清理旧临时文件
                for f in os.listdir("."):
                    if f.startswith("_chunk_") and f.endswith(".bin"):
                        os.remove(f)

                chunks = []
                empty_streak = 0  # 连续失败计数
                for i in range(total_chunks):
                    chunk_addr = addr + i * self.CHUNK_SIZE
                    chunk_size = min(self.CHUNK_SIZE, size - i * self.CHUNK_SIZE)
                    chunk_file = f"_chunk_{i:04d}.bin"

                    msg = f"[{i+1}/{total_chunks}] 0x{chunk_addr:06X} +{chunk_size//1024}KB ..."
                    self.root.after(0, lambda m=msg: self._log(w, m))
                    self.root.after(0, lambda p=i+1, t=total_chunks: self._status(f"读取中... {p}/{t}"))

                    ok = False
                    for attempt in range(3):
                        try:
                            run_esptool(["--chip", "esp32s3", "--port", port,
                                         "--baud", "115200",
                                         "--before", "default_reset",
                                         "--after", "hard_reset",
                                         "read_flash",
                                         hex(chunk_addr), hex(chunk_size), chunk_file],
                                        timeout=30)
                            ok = True
                            break
                        except Exception:
                            if attempt < 2:
                                time.sleep(2.0)

                    if ok:
                        chunks.append((chunk_addr, chunk_file))
                        empty_streak = 0
                    else:
                        # 本块读不到 → 标记空块，继续试下一块
                        empty_streak += 1
                        self.root.after(0, lambda: self._log(w, f"  → 空块，跳过"))
                        # 删除失败文件
                        for f2 in os.listdir("."):
                            if f2 == chunk_file:
                                os.remove(f2)
                        if empty_streak >= 5:
                            self.root.after(0, lambda: self._log(
                                w, f"  → 连续 {empty_streak} 个空块，停止"))
                            break

                # 清理未使用的临时文件
                used = set(cf for _, cf in chunks)
                for f in os.listdir("."):
                    if f.startswith("_chunk_") and f.endswith(".bin") and f not in used:
                        os.remove(f)

                # 拼接 + 补零（处理块间空隙）
                self.root.after(0, lambda: self._log(w, "拼接中..."))
                self.root.after(0, lambda: self._status("拼接文件中..."))
                written = 0
                with open(out, "wb") as outf:
                    for chunk_addr, cf in chunks:
                        gap = chunk_addr - (addr + written)
                        if gap > 0:
                            outf.write(b'\x00' * gap)
                            self.root.after(0, lambda g=gap: self._log(
                                w, f"  空隙 0x{chunk_addr - g:06X}-0x{chunk_addr:06X} 补零 {g} bytes"))
                            written += gap
                        with open(cf, "rb") as inf:
                            data = inf.read()
                            outf.write(data)
                            written += len(data)
                        os.remove(cf)
                    remaining = size - written
                    if remaining > 0:
                        outf.write(b'\x00' * remaining)
                        self.root.after(0, lambda r=remaining: self._log(
                            w, f"  末尾补零 {r // 1024}KB（未写入区域）"))

                sz = os.path.getsize(out)
                self.root.after(0, lambda: self._log(w, f"✓ 成功! 文件 {sz} bytes → {out}"))
                enc, ent, reason = is_likely_encrypted(out)
                self.root.after(0, lambda: self._log(
                    w, f"  安全检查: 熵={ent:.2f}  加密={'是' if enc else '否'} ({reason})"))
                self.root.after(0, lambda: self._status("提取完成"))
            except Exception as e:
                self.root.after(0, lambda: self._log(w, f"✗ {e}"))
                self.root.after(0, lambda: self._status("提取失败"))

        threading.Thread(target=task, daemon=True).start()

    # ==================== 验证 ====================

    def _do_verify(self):
        port = self._port()
        if not port:
            return
        addr = self.va.get().strip()
        size = self.vs.get().strip()
        w = self.vlog

        self._log(w, f"验证: {port}  addr={addr}  size={size}")

        def task():
            try:
                tmp = "_gui_verify.bin"
                self._status("读取 Flash 样本...")
                run_esptool(["--chip", "esp32s3", "--port", port, "--baud", "115200",
                             "read_flash", addr, size, tmp])
                enc, ent, reason = is_likely_encrypted(tmp)
                os.remove(tmp)

                def update():
                    if enc:
                        self.vresult.set(f"✓ Flash Encryption 已生效\n熵值: {ent:.2f} / 8.0\n{reason}")
                        self.vframe.configure(text="验证结果 — 已加密")
                    else:
                        self.vresult.set(f"✗ Flash 未加密\n熵值: {ent:.2f} / 8.0\n{reason}")
                        self.vframe.configure(text="验证结果 — 明文")
                    self._log(w, f"熵值={ent:.4f}  判断: {reason}")
                    self._status("验证完成")
                self.root.after(0, update)
            except Exception as e:
                self.root.after(0, lambda: self._log(w, f"✗ {e}"))
                self.root.after(0, lambda: self._status("验证失败"))

        threading.Thread(target=task, daemon=True).start()

    # ==================== SPIFFS 解包 ====================

    def _build_spiffs_tab(self):
        f = ttk.Frame(self.tab_spiffs)
        f.pack(fill=tk.X, pady=(0, 10))

        ttk.Label(f, text="镜像文件:").grid(row=0, column=0, sticky=tk.W, pady=2)
        self.spiffs_img = ttk.Entry(f, width=42)
        self.spiffs_img.grid(row=0, column=1, columnspan=2, padx=5, pady=2)
        ttk.Button(f, text="浏览...", width=7, command=self._browse_spiffs).grid(row=0, column=3, padx=5, pady=2)

        ttk.Label(f, text="输出目录:").grid(row=1, column=0, sticky=tk.W, pady=2)
        self.spiffs_out = ttk.Entry(f, width=42)
        self.spiffs_out.insert(0, "spiffs_extracted")
        self.spiffs_out.grid(row=1, column=1, columnspan=2, padx=5, pady=2)
        ttk.Button(f, text="浏览...", width=7, command=self._browse_spiffs_out).grid(row=1, column=3, padx=5, pady=2)

        # SPIFFS 参数（一般不需要改）
        pf = ttk.LabelFrame(self.tab_spiffs, text="SPIFFS 参数（默认即可）", padding="8")
        pf.pack(fill=tk.X, pady=(0, 10))

        pf.columnconfigure(1, weight=1)
        pf.columnconfigure(4, weight=1)

        ttk.Label(pf, text="块大小:").grid(row=0, column=0, sticky=tk.W)
        self.s_block = ttk.Entry(pf, width=12)
        self.s_block.insert(0, "4096")
        self.s_block.grid(row=0, column=1, padx=5, sticky=tk.W)

        ttk.Label(pf, text="页大小:").grid(row=0, column=2, sticky=tk.W, padx=(20, 0))
        self.s_page = ttk.Entry(pf, width=12)
        self.s_page.insert(0, "256")
        self.s_page.grid(row=0, column=3, padx=5, sticky=tk.W)

        ttk.Label(pf, text="镜像大小 (0=自动):").grid(row=1, column=0, sticky=tk.W)
        self.s_size = ttk.Entry(pf, width=12)
        self.s_size.insert(0, "0")
        self.s_size.grid(row=1, column=1, padx=5, sticky=tk.W)

        ttk.Label(pf, text="留 0 则自动取文件大小", foreground="gray").grid(
            row=1, column=2, columnspan=2, padx=5, sticky=tk.W)

        ttk.Button(self.tab_spiffs, text="解包 SPIFFS", width=16, command=self._do_unpack).pack(pady=8)

        ttk.Label(self.tab_spiffs, text="输出:").pack(anchor=tk.W)
        self.slog = scrolledtext.ScrolledText(self.tab_spiffs, height=14, state=tk.DISABLED, font=("Consolas", 9))
        self.slog.pack(fill=tk.BOTH, expand=True)

    def _browse_spiffs(self):
        p = filedialog.askopenfilename(filetypes=[("Bin", "*.bin"), ("All", "*.*")])
        if p:
            self.spiffs_img.delete(0, tk.END)
            self.spiffs_img.insert(0, p)

    def _browse_spiffs_out(self):
        p = filedialog.askdirectory()
        if p:
            self.spiffs_out.delete(0, tk.END)
            self.spiffs_out.insert(0, p)

    def _do_unpack(self):
        img = self.spiffs_img.get().strip()
        out = self.spiffs_out.get().strip() or "spiffs_extracted"
        w = self.slog

        if not img or not os.path.exists(img):
            messagebox.showwarning("文件不存在", f"找不到镜像文件: {img}")
            return

        block_size = int(self.s_block.get() or "4096")
        page_size = int(self.s_page.get() or "256")
        img_size = int(self.s_size.get() or "0") or os.path.getsize(img)

        import importlib.util
        # 尝试导入纯 Python SPIFFS 解包器
        unpacker_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "unpack_spiffs.py")
        spec = importlib.util.spec_from_file_location("unpack_spiffs", unpacker_path)
        unpack_mod = importlib.util.module_from_spec(spec)

        os.makedirs(out, exist_ok=True)

        self._log(w, f"{'='*50}")
        self._log(w, f"镜像: {img} ({img_size} bytes)")
        self._log(w, f"参数: block={block_size}  page={page_size}")
        self._log(w, f"输出: {out}/")

        def task():
            try:
                self._status("解包中...")
                # 动态加载并执行纯 Python 解包器
                spec.loader.exec_module(unpack_mod)
                count = unpack_mod.unpack_spiffs(img, out, block_size, page_size)
                self.root.after(0, lambda: self._log(w, f"✓ 解包成功！共 {count} 个文件"))
                self.root.after(0, lambda: self._status("解包完成"))
            except Exception as e:
                self.root.after(0, lambda: self._log(w, f"✗ 解包失败：{e}"))
                self.root.after(0, lambda: self._status("解包失败"))

        threading.Thread(target=task, daemon=True).start()


def main():
    root = tk.Tk()
    FlashToolGUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()
