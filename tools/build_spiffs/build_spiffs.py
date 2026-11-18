"""
SPIFFS 镜像打包工具

用法:
    python build_spiffs.py                          # GUI 模式（无参数时）
    python build_spiffs.py -s 1024000 -o spiffs.bin # CLI 模式
    python build_spiffs.py -d E:\my_webui            # CLI 模式
"""

import os
import sys
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from spiffsgen import SpiffsFS, SpiffsBuildConfig
from spiffsgen import SPIFFS_PAGE_IX_LEN, SPIFFS_BLOCK_IX_LEN, SPIFFS_OBJ_ID_LEN, SPIFFS_SPAN_IX_LEN

# ── 路径 ──
if getattr(sys, 'frozen', False):
    EXE_DIR = os.path.dirname(os.path.abspath(sys.executable))
else:
    EXE_DIR = os.path.dirname(os.path.abspath(__file__))

# ── 默认 SPIFFS 参数（与 sdkconfig 保持一致） ──
DEFAULT_IMAGE_SIZE = 1000 * 1024  # 1000K
DEFAULT_PAGE_SIZE = 256
DEFAULT_BLOCK_SIZE = 4096
DEFAULT_OBJ_NAME_LEN = 32
DEFAULT_META_LEN = 4
DEFAULT_USE_MAGIC = True
DEFAULT_USE_MAGIC_LEN = True
DEFAULT_BIG_ENDIAN = False
DEFAULT_ALIGNED_OBJ_IX_TABLES = False


# ==================== 自动检测 webui 目录 ====================

def _has_web_files(directory):
    if not os.path.isdir(directory):
        return False
    try:
        for f in os.listdir(directory):
            if f.lower().endswith(('.html', '.htm')):
                if os.path.isfile(os.path.join(directory, f)):
                    return True
    except PermissionError:
        pass
    return False


def auto_detect_webui():
    """
    从 EXE 当前目录向上最多 3 层，查找名为 webui 或包含 .html 的目录。
    返回路径列表。
    """
    candidates = []
    seen = set()

    def add(full_path):
        norm = os.path.normcase(full_path)
        if norm not in seen and _has_web_files(full_path):
            candidates.append(full_path)
            seen.add(norm)

    # 当前目录本身
    add(EXE_DIR)
    # 当前目录下名为 webui 的子目录
    add(os.path.join(EXE_DIR, "webui"))

    # 向上扫描
    current = EXE_DIR
    for _ in range(3):
        parent = os.path.dirname(current)
        if parent == current:
            break
        add(parent)
        add(os.path.join(parent, "webui"))
        current = parent

    return candidates


def resolve_best_src():
    """选最佳源目录，优先路径以 webui 结尾的"""
    candidates = auto_detect_webui()
    if not candidates:
        return ""
    for p in candidates:
        if os.path.basename(p).lower() == 'webui':
            return p
    return candidates[0]


def make_output_path(src_dir):
    """输出文件 = EXE 当前目录 / spiffs.bin"""
    return os.path.join(EXE_DIR, "spiffs.bin")


# ==================== 构建核心 ====================

def build_spiffs(image_size, base_dir, output_file,
                 page_size=DEFAULT_PAGE_SIZE,
                 block_size=DEFAULT_BLOCK_SIZE,
                 obj_name_len=DEFAULT_OBJ_NAME_LEN,
                 meta_len=DEFAULT_META_LEN,
                 use_magic=DEFAULT_USE_MAGIC,
                 use_magic_len=DEFAULT_USE_MAGIC_LEN,
                 big_endian=DEFAULT_BIG_ENDIAN,
                 aligned_obj_ix_tables=DEFAULT_ALIGNED_OBJ_IX_TABLES,
                 log_callback=None):
    """构建 SPIFFS 镜像"""

    def log(msg):
        print(msg)
        if log_callback:
            log_callback(msg + '\n')

    if not os.path.isdir(base_dir):
        raise FileNotFoundError(f"源目录不存在: {base_dir}")

    log(f"源目录:   {base_dir}")
    log(f"输出文件: {output_file}")
    log(f"镜像大小: {image_size} bytes ({image_size // 1024}K)")
    log(f"Page: {page_size}, Block: {block_size}, NameLen: {obj_name_len}, Meta: {meta_len}")
    log(f"Magic: {use_magic}, MagicLen: {use_magic_len}")

    config = SpiffsBuildConfig(
        page_size, SPIFFS_PAGE_IX_LEN,
        block_size, SPIFFS_BLOCK_IX_LEN,
        meta_len, obj_name_len,
        SPIFFS_OBJ_ID_LEN, SPIFFS_SPAN_IX_LEN,
        True, True,
        'big' if big_endian else 'little',
        use_magic, use_magic_len,
        aligned_obj_ix_tables
    )

    spiffs = SpiffsFS(image_size, config)

    file_count = 0
    for root, dirs, files in os.walk(base_dir):
        for f in files:
            full_path = os.path.join(root, f)
            vfs_path = '/' + os.path.relpath(full_path, base_dir).replace('\\', '/')
            spiffs.create_file(vfs_path, full_path)
            log(f"  + {vfs_path}")
            file_count += 1

    log(f"\n共打包 {file_count} 个文件，正在生成镜像...")

    os.makedirs(os.path.dirname(output_file) or '.', exist_ok=True)
    with open(output_file, 'wb') as f:
        f.write(spiffs.to_binary())

    actual_size = os.path.getsize(output_file)
    usage_pct = actual_size / image_size * 100
    log(f"镜像已生成: {output_file} ({actual_size} bytes, 分区占用 {usage_pct:.1f}%)")

    return file_count


# ==================== GUI 模式 ====================

def run_gui():
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk

    root = tk.Tk()
    root.title("SPIFFS 镜像打包工具")
    root.resizable(False, False)

    SIZE_PRESETS = {
        "1000K (默认)": 1000 * 1024,
        "512K": 512 * 1024,
        "1M": 1024 * 1024,
        "2M": 2 * 1024 * 1024,
        "自定义": 0,
    }

    best_src = resolve_best_src()
    detected = auto_detect_webui()

    # ── 源目录 ──
    ttk.Label(root, text="源目录（网页文件夹）:").grid(
        row=0, column=0, sticky='w', padx=10, pady=(10, 2))
    src_frame = ttk.Frame(root)
    src_frame.grid(row=1, column=0, columnspan=2, sticky='ew', padx=10)
    src_var = tk.StringVar(value=best_src)
    src_combo = ttk.Combobox(src_frame, textvariable=src_var, width=57)
    src_combo.pack(side='left', fill='x', expand=True)
    if detected:
        src_combo['values'] = detected

    def on_src_changed(*_):
        out_var.set(make_output_path(src_var.get()))

    src_var.trace_add('write', on_src_changed)

    def browse_dir():
        initial = src_var.get() if os.path.isdir(src_var.get()) else EXE_DIR
        path = filedialog.askdirectory(title="选择网页源目录", initialdir=initial)
        if path:
            src_var.set(path)

    ttk.Button(src_frame, text="浏览...", command=browse_dir).pack(side='left', padx=(5, 0))

    # ── 输出文件 ──
    ttk.Label(root, text="输出文件:").grid(row=2, column=0, sticky='w', padx=10, pady=(10, 2))
    out_frame = ttk.Frame(root)
    out_frame.grid(row=3, column=0, columnspan=2, sticky='ew', padx=10)
    out_var = tk.StringVar(value=make_output_path(best_src))
    ttk.Entry(out_frame, textvariable=out_var, width=60).pack(side='left', fill='x', expand=True)

    def browse_save():
        path = filedialog.asksaveasfilename(
            title="保存 SPIFFS 镜像",
            initialdir=EXE_DIR,
            initialfile=os.path.basename(out_var.get()),
            defaultextension=".bin",
            filetypes=[("Binary files", "*.bin"), ("All files", "*.*")]
        )
        if path:
            out_var.set(path)

    ttk.Button(out_frame, text="浏览...", command=browse_save).pack(side='left', padx=(5, 0))

    # ── 镜像大小 ──
    ttk.Label(root, text="镜像大小:").grid(row=4, column=0, sticky='w', padx=10, pady=(10, 2))
    size_frame = ttk.Frame(root)
    size_frame.grid(row=5, column=0, columnspan=2, sticky='ew', padx=10)
    size_preset_var = tk.StringVar(value="1000K (默认)")
    size_var = tk.StringVar(value=str(DEFAULT_IMAGE_SIZE))

    def on_preset_change(*_):
        preset = size_preset_var.get()
        if preset != "自定义":
            size_var.set(str(SIZE_PRESETS[preset]))

    size_preset_var.trace_add('write', on_preset_change)
    ttk.OptionMenu(size_frame, size_preset_var, "1000K (默认)", *SIZE_PRESETS.keys()).pack(side='left')
    ttk.Label(size_frame, text=" bytes:").pack(side='left', padx=(10, 2))
    ttk.Entry(size_frame, textvariable=size_var, width=12).pack(side='left')

    # ── 构建按钮 ──
    btn_frame = ttk.Frame(root)
    btn_frame.grid(row=6, column=0, columnspan=2, pady=(15, 5))
    ttk.Button(btn_frame, text="开始构建", command=lambda: do_build()).pack()

    # ── 输出日志 ──
    ttk.Label(root, text="输出日志:").grid(row=7, column=0, sticky='w', padx=10, pady=(5, 2))
    log_text = tk.Text(root, width=78, height=14, state='disabled', font=('Consolas', 9))
    log_text.grid(row=8, column=0, columnspan=2, padx=10, pady=(0, 10))

    scrollbar = ttk.Scrollbar(root, command=log_text.yview)
    scrollbar.grid(row=8, column=2, sticky='ns', padx=(0, 10), pady=(0, 10))
    log_text.configure(yscrollcommand=scrollbar.set)

    def append_log(msg):
        log_text.configure(state='normal')
        log_text.insert('end', msg)
        log_text.see('end')
        log_text.configure(state='disabled')

    def do_build():
        src = src_var.get().strip()
        out = out_var.get().strip()
        size_str = size_var.get().strip()

        if not src:
            messagebox.showerror("错误", "请选择源目录")
            return
        if not out:
            messagebox.showerror("错误", "请指定输出文件路径")
            return

        try:
            image_size = int(size_str, 0)
        except ValueError:
            messagebox.showerror("错误", f"无法解析镜像大小: {size_str}")
            return
        if image_size <= 0:
            messagebox.showerror("错误", "镜像大小必须大于 0")
            return

        log_text.configure(state='normal')
        log_text.delete('1.0', 'end')
        log_text.configure(state='disabled')

        def build_thread():
            try:
                build_spiffs(image_size, src, out, log_callback=append_log)
                append_log("\n完成!\n")
            except Exception as e:
                append_log(f"\n错误: {e}\n")

        threading.Thread(target=build_thread, daemon=True).start()

    root.mainloop()


# ==================== CLI 模式 ====================

def run_cli():
    import argparse

    best_src = resolve_best_src()

    parser = argparse.ArgumentParser(
        description='SPIFFS 镜像打包工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=f"""
示例:
  python build_spiffs.py                          # GUI 模式
  python build_spiffs.py -s 0xFA000 -o D:\\spiffs_0.bin
  python build_spiffs.py -d E:\\my_webui -o E:\\spiffs.bin

自动检测源目录: {best_src if best_src else '(未找到，请用 -d 指定)'}
输出目录:       {EXE_DIR}
        """
    )
    parser.add_argument('-d', '--dir', default=best_src, help='网页源目录')
    parser.add_argument('-o', '--output', default=None,
                        help=f'输出镜像文件路径 (默认: 当前目录/<源文件夹名>.bin)')
    parser.add_argument('-s', '--size', default=str(DEFAULT_IMAGE_SIZE),
                        help=f'镜像大小 (默认: {DEFAULT_IMAGE_SIZE // 1024}K)')

    args = parser.parse_args()

    if not args.dir:
        print("错误: 未找到网页源目录，请用 -d 指定")
        sys.exit(1)

    output = args.output or make_output_path(args.dir)

    try:
        image_size = int(args.size, 0)
    except ValueError:
        print(f"错误: 无法解析镜像大小 '{args.size}'")
        sys.exit(1)

    try:
        build_spiffs(image_size, args.dir, output)
        print("\n完成!")
    except Exception as e:
        print(f"\n错误: {e}")
        sys.exit(1)


if __name__ == '__main__':
    if len(sys.argv) > 1:
        run_cli()
    else:
        run_gui()
