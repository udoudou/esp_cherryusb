/*
 * Copyright (c) 2024, udoudou
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <time.h>
#include <stdint.h>

#define MTP_FILE_LIST_SIZE 64

typedef struct {
    uint16_t storage_id;
    uint16_t parent;
    char *name;
}esp_mtp_file_entry_t;

typedef struct esp_mtp_file_list {
    esp_mtp_file_entry_t entry_list[MTP_FILE_LIST_SIZE];
    struct esp_mtp_file_list *next;
}esp_mtp_file_list_t;

typedef struct {
    uint32_t count;
    esp_mtp_file_list_t list;
}esp_mtp_file_handle_list_t;

/** @brief UTF8 转 UTF16
 *
 * @param utf16 需要转换的 UTF8 字符串
 * @param[out] out 保存转换后的 UTF16 字符串
 * @param[in out] len 输入保存空间的长度，输出完成转换的 UTF16 字符数（含结束符）
 *
 * @return 保存结束符的下一个内存地址
 */
char *esp_mtp_utf8_to_utf16(const char *utf8, char *out, uint8_t *len);

char *esp_mtp_time_to_utf16_datatime(time_t time, char *out, uint8_t *len);

/** @brief UTF16 转 UTF8
 *
 * @param utf16 需要转换的 UTF16 字符串
 * @param[out] out 保存转换后的 UTF8 字符串
 * @param[in out] len 输入保存空间的长度，输出完成转换的 UTF8 字符数（含结束符）
 *
 * @return 保存结束符的下一个内存地址
 */
char *esp_mtp_utf16_to_utf8(const char *utf16, char *out, uint8_t *len);

time_t esp_mtp_utf16_datatime_to_time(const char *utf16);

void esp_mtp_file_list_init(esp_mtp_file_handle_list_t *file_list);

uint32_t esp_mtp_file_list_add(esp_mtp_file_handle_list_t *file_list, uint32_t storage_id, uint32_t parent, const char *name);

const esp_mtp_file_entry_t *esp_mtp_file_list_find(esp_mtp_file_handle_list_t *file_list, uint32_t handle, char *path, uint32_t max_len);

void esp_mtp_file_list_clean(esp_mtp_file_handle_list_t *file_list);

uint8_t *esp_mtp_file_list_fill_handle_array(esp_mtp_file_handle_list_t *file_list, uint32_t parent, uint8_t *out, uint32_t *len);