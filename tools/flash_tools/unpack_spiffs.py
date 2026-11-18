#!/usr/bin/env python3
"""
纯 Python SPIFFS 解包器（基于 ESP-IDF spiffsgen.py 格式）
==========================================================
完全参考 esp-idf/components/spiffs/spiffsgen.py 的二进制布局实现反向解析。

用法：
    python unpack_spiffs.py <spiffs_dump.bin> -o <输出目录>
"""

import argparse
import math
import os
import re
import struct

# ==================== 来自 spiffsgen.py 的常量 ====================

SPIFFS_PH_FLAG_USED_FINAL_INDEX = 0xF8  # 索引页标记
SPIFFS_PH_FLAG_USED_FINAL = 0xFC        # 数据页标记
SPIFFS_TYPE_FILE = 1                     # 文件类型

# 默认字段长度（与 ESP-IDF 默认 sdkconfig 一致）
OBJ_ID_LEN = 2         # spiffs_obj_id
SPAN_IX_LEN = 2        # spiffs_span_ix
PH_FLAG_LEN = 1         # 页 flags
PH_IX_SIZE_LEN = 4      # 文件大小
PH_IX_OBJ_TYPE_LEN = 1  # 对象类型
OBJ_NAME_LEN = 32       # CONFIG_SPIFFS_OBJ_NAME_LEN
META_LEN = 4            # CONFIG_SPIFFS_META_LENGTH
PAGE_IX_LEN = 2         # spiffs_page_ix

# 数据页头 = obj_id(2) + span_ix(2) + flags(1) = 5
DATA_PAGE_HDR_LEN = OBJ_ID_LEN + SPAN_IX_LEN + PH_FLAG_LEN  # = 5
# 索引页头对齐到 4 的倍数 → DATA_PAGE_HDR_LEN_ALIGNED = 8, pad = 3
_DATA_PAD = 4 - (DATA_PAGE_HDR_LEN % 4)  # = 3
DATA_PAGE_HDR_ALIGNED = DATA_PAGE_HDR_LEN + _DATA_PAD  # = 8
DATA_PAGE_CONTENT_LEN = 256 - DATA_PAGE_HDR_LEN  # = 251

# 索引页元数据区 = 对齐头(8) + 文件大小(4) + 类型(1) + 文件名(32) + 元数据(4) = 49
INDEX_META_OFFSET = DATA_PAGE_HDR_ALIGNED  # = 8
INDEX_NAME_OFFSET = INDEX_META_OFFSET + PH_IX_SIZE_LEN + PH_IX_OBJ_TYPE_LEN  # = 13
INDEX_PAGE_DATA_OFFSET = INDEX_NAME_OFFSET + OBJ_NAME_LEN + META_LEN  # = 49


def _valid_filename(name):
    """过滤非法字符"""
    if not name or name in (".", ".."):
        return None
    name = re.sub(r'[\\/*?:"<>|\x00-\x1f\x7f-\x9f]', '_', name)
    name = name.strip().strip('.')
    if not name or len(name) > 128:
        return None
    return name


def unpack_spiffs(image_path, output_dir, block_size=4096, page_size=256):
    """解包 SPIFFS 镜像"""
    with open(image_path, "rb") as f:
        data = f.read()

    total_pages = len(data) // page_size
    print(f"镜像大小: {len(data)} bytes, {total_pages} 页 (page={page_size})")

    # 收集所有 index page（含文件名）和 data page
    # objects = {obj_id: {"name": ..., "size": ..., "data_pages": [(span_ix, page_offset)]}}
    objects = {}

    for page_num in range(total_pages):
        page_off = page_num * page_size
        flags = data[page_off + DATA_PAGE_HDR_LEN - PH_FLAG_LEN]  # offset 4

        if flags == 0xFF:
            continue  # 空页

        obj_id = struct.unpack_from("<H", data, page_off)[0]
        span_ix = struct.unpack_from("<H", data, page_off + OBJ_ID_LEN)[0]

        if flags == SPIFFS_PH_FLAG_USED_FINAL_INDEX:
            # 索引页：obj_id 在页内做了 MSB 翻转，需还原
            obj_id ^= 0x8000

            if obj_id not in objects:
                objects[obj_id] = {"name": None, "size": 0, "data_pages": [], "index_pages": []}
            objects[obj_id]["index_pages"].append(page_num)

            if span_ix == 0:
                # 第一个索引页含文件元数据
                file_size = struct.unpack_from("<I", data, page_off + INDEX_META_OFFSET)[0]
                file_type = data[page_off + INDEX_META_OFFSET + PH_IX_SIZE_LEN]
                if file_type == SPIFFS_TYPE_FILE:
                    raw_name = data[page_off + INDEX_NAME_OFFSET:
                                    page_off + INDEX_NAME_OFFSET + OBJ_NAME_LEN]
                    name = raw_name.rstrip(b'\x00').decode("utf-8", errors="replace")
                    name = _valid_filename(name)
                    objects[obj_id]["name"] = name
                objects[obj_id]["size"] = file_size

            # 读取此索引页指向的数据页列表
            indices_start = page_off + INDEX_PAGE_DATA_OFFSET if span_ix == 0 else page_off + DATA_PAGE_HDR_ALIGNED
            indices_end = page_off + page_size
            idx = indices_start
            while idx + PAGE_IX_LEN <= indices_end:
                page_idx = struct.unpack_from("<H", data, idx)[0]
                idx += PAGE_IX_LEN
                if page_idx == 0xFFFF:
                    break
                objects[obj_id]["data_pages"].append((len(objects[obj_id]["data_pages"]), page_idx, 0))
                # ^ (span_ix will be filled from data pages)

        elif flags == SPIFFS_PH_FLAG_USED_FINAL:
            # 数据页
            if obj_id not in objects:
                objects[obj_id] = {"name": None, "size": 0, "data_pages": [], "index_pages": []}
            objects[obj_id]["data_pages"].append((span_ix, page_num, page_off))

    os.makedirs(output_dir, exist_ok=True)
    extracted = 0

    for obj_id in sorted(objects.keys()):
        obj = objects[obj_id]
        data_entries = sorted(obj["data_pages"], key=lambda x: x[0])  # 按 span_ix 排序

        if not data_entries:
            continue

        name = obj["name"] or f"_obj_{obj_id:04x}.bin"

        out_path = os.path.join(output_dir, name.lstrip("/"))
        out_parent = os.path.dirname(out_path)
        if out_parent:
            os.makedirs(out_parent, exist_ok=True)

        with open(out_path, "wb") as out_f:
            for span_ix, page_num, page_off in data_entries:
                content_start = page_off + DATA_PAGE_HDR_LEN
                content_size = page_size - DATA_PAGE_HDR_LEN
                end = min(content_start + content_size, len(data))
                if end > content_start:
                    out_f.write(data[content_start:end])

        fsize = os.path.getsize(out_path)
        # 截断到实际大小
        if obj["size"] > 0 and fsize > obj["size"]:
            with open(out_path, "rb+") as f:
                f.truncate(obj["size"])
            fsize = obj["size"]

        print(f"  {name}  ({fsize} bytes)")
        extracted += 1

    print(f"\n共提取 {extracted} 个文件 → {output_dir}/")
    return extracted


def main():
    parser = argparse.ArgumentParser(description="纯 Python SPIFFS 解包器")
    parser.add_argument("image", help="SPIFFS 镜像文件 (.bin)")
    parser.add_argument("-o", "--out", default="spiffs_extracted", help="输出目录")
    args = parser.parse_args()

    if not os.path.exists(args.image):
        print(f"错误：文件不存在: {args.image}")
        return

    unpack_spiffs(args.image, args.out)


if __name__ == "__main__":
    main()
