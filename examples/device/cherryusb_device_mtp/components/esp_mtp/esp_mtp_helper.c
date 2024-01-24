/*
 * Copyright (c) 2024, udoudou
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_mtp_helper.h"

#include "string.h"
#include <stdio.h>
#include "inttypes.h"

uint8_t *esp_mtp_utf8_to_utf16(const char *utf8, uint8_t *out, uint8_t *len)
{
    uint16_t *out_start = (uint16_t *)out;
    uint16_t *out_end = out_start;
    while (*utf8 != '\0') {
        uint16_t temp;
        temp = *(utf8++);
        if (temp & 0x80) {
            uint8_t first_byte = temp;
            if ((first_byte & 0xE0) == 0xC0) {
                temp = (first_byte & 0x1F) << 6;
                first_byte = *(utf8++);
                if ((first_byte & 0xC0) != 0x80) {
                    break;
                }
                temp |= (first_byte & 0x3F);
            } else if ((first_byte & 0xF0) == 0xE0) {
                temp = (first_byte & 0x0F) << 12;
                first_byte = *(utf8++);
                if ((first_byte & 0xC0) != 0x80) {
                    break;
                }
                temp |= ((first_byte & 0x3F) << 6);
                first_byte = *(utf8++);
                if ((first_byte & 0xC0) != 0x80) {
                    break;
                }
                temp |= (first_byte & 0x3F);
            } else {
                break;
            }
        }
        *(out_end++) = temp;
    }
    *(out_end++) = 0x0000;
    *len = out_end - out_start;
    return (uint8_t *)out_end;
}

uint8_t *esp_mtp_utf16_to_utf8(const char *utf16, uint8_t *out, uint8_t *len)
{
    uint16_t *utf16_ptr = (uint16_t *)utf16;
    uint8_t *out_start = out;
    while (*utf16_ptr != 0x0) {
        uint16_t temp = *(utf16_ptr++);
        if (temp <= 0x7F) {
            *(out++) = temp;
            continue;
        } else if (temp <= 0x7FF) {
            *(out++) = (temp >> 6) | 0xC0;
            *(out++) = (temp & 0x3F) | 0x80;
            continue;
        }
        *(out++) = (temp >> 12) | 0xE0;
        *(out++) = ((temp >> 6) & 0x3F) | 0x80;
        *(out++) = (temp & 0x3F) | 0x80;
    }
    *(out++) = 0x00;
    *len = (out - out_start);
    return out;
}

void esp_mtp_file_list_init(esp_mtp_file_handle_list_t *file_list)
{
    memset(file_list, 0, sizeof(esp_mtp_file_handle_list_t));
}

uint32_t esp_mtp_file_list_add(esp_mtp_file_handle_list_t *file_list, uint32_t storage_id, uint32_t parent, const char *name)
{
    esp_mtp_file_list_t *list;
    list = &file_list->list;
    for (uint32_t i = 1; i <= file_list->count / MTP_FILE_LIST_SIZE; i++) {
        if (list->next == NULL) {
            list->next = (esp_mtp_file_list_t *)calloc(1, sizeof(esp_mtp_file_list_t));
            if (list->next == NULL) {
                return 0;
            }
        }
        list = list->next;
    }
    list->entry_list[file_list->count % MTP_FILE_LIST_SIZE].name = (char *)malloc(strlen(name) + 1);
    if (list->entry_list[file_list->count % MTP_FILE_LIST_SIZE].name == NULL) {
        return 0;
    }
    strcpy(list->entry_list[file_list->count % MTP_FILE_LIST_SIZE].name, name);
    list->entry_list[file_list->count % MTP_FILE_LIST_SIZE].storage_id = storage_id;
    list->entry_list[file_list->count % MTP_FILE_LIST_SIZE].parent = parent;
    file_list->count++;
    // printf("add hande: %"PRIu32"(%"PRIu32")\n", file_list->count, parent);
    return file_list->count;
}

const esp_mtp_file_entry_t *esp_mtp_file_list_find(esp_mtp_file_handle_list_t *file_list, uint32_t handle, char *path, uint32_t max_len)
{
    esp_mtp_file_entry_t *entry;
    esp_mtp_file_list_t *list;
    uint32_t path_len = 0;

    if (handle > file_list->count) {
        return NULL;
    }
    handle = handle - 1;
    list = &file_list->list;
    for (uint32_t i = 1; i <= handle / MTP_FILE_LIST_SIZE; i++) {
        list = list->next;
    }
    entry = &list->entry_list[handle % MTP_FILE_LIST_SIZE];
    if (entry->name == NULL) {
        return NULL;
    }
    // printf("find: %"PRIu32"(%"PRIu16")\n", handle + 1, entry->parent);
    if (entry->parent != 0) {
        if (esp_mtp_file_list_find(file_list, entry->parent, path, max_len) == NULL) {
            return NULL;
        }
        path_len = strlen(path);
    }

    if (path_len + strlen(entry->name) + 2 > max_len) {
        return NULL;
    }
    path[path_len++] = '/';
    strcpy(path + path_len, entry->name);
    return entry;
}

uint8_t *esp_mtp_file_list_fill_handle_array(esp_mtp_file_handle_list_t *file_list, uint32_t parent, uint8_t *out, uint32_t *len)
{
    uint32_t max_count;
    uint32_t count = 0;
    esp_mtp_file_list_t *list;

    if (parent < file_list->count) {
        list = &file_list->list;
        for (uint32_t i = 1; i <= parent / MTP_FILE_LIST_SIZE; i++) {
            list = list->next;
        }
        max_count = *len / sizeof(uint32_t);
        for (uint32_t i = parent; i < file_list->count; i++) {
            if (list->entry_list[i % MTP_FILE_LIST_SIZE].parent == parent) {
                *(uint32_t *)out = i + 1;
                out += sizeof(uint32_t);
                count++;
                if (count == max_count) {
                    break;;
                }
            }
        }
    }

    *len = count;
    return out;
}