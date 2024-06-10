/*
 * Copyright (c) 2024, udoudou
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_mtp_helper.h"

#include "string.h"
#include <stdio.h>
#include "inttypes.h"

#ifdef CONFIG_SPIRAM_BOOT_INIT
#include "esp_heap_caps.h"
#endif

char *esp_mtp_utf8_to_utf16(const char *utf8, char *out, uint8_t *len)
{
    uint16_t *utf16_ptr = (uint16_t *)out;
    uint16_t *utf16_end_ptr = utf16_ptr + (*len / 2) - 1;
    while (*utf8 != '\0' && utf16_ptr < utf16_end_ptr) {
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
        *(utf16_ptr++) = temp;
    }
    *(utf16_ptr++) = 0x0000;
    *len = utf16_ptr - (uint16_t *)out;
    return (char *)utf16_ptr;
}

char *esp_mtp_time_to_utf16_datatime(time_t time, char *out, uint8_t *len)
{
    struct tm timeinfo;
    char buff[24];
    localtime_r(&time, &timeinfo);
    // snprintf(buff, sizeof(buff), "%.4d%.2d%.2dT%.2d%.2d%.2d", 1900 + timeinfo.tm_year, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    strftime(buff, sizeof(buff), "%Y%m%dT%H%M%S", &timeinfo);
    return esp_mtp_utf8_to_utf16(buff, out, len);
}

char *esp_mtp_utf16_to_utf8(const char *utf16, char *out, uint8_t *len)
{
    uint16_t *utf16_ptr = (uint16_t *)utf16;
    char *out_end = out + *len - 1;
    *len = 1;
    while (*utf16_ptr != 0x0) {
        uint16_t temp = *(utf16_ptr++);
        if (temp <= 0x7F) {
            if (out >= out_end) {
                break;
            }
            *(out++) = temp;
        } else if (temp <= 0x7FF) {
            if (out + 1 >= out_end) {
                break;
            }
            *(out++) = (temp >> 6) | 0xC0;
            *(out++) = (temp & 0x3F) | 0x80;
        } else {
            if (out + 2 >= out_end) {
                break;
            }
            *(out++) = (temp >> 12) | 0xE0;
            *(out++) = ((temp >> 6) & 0x3F) | 0x80;
            *(out++) = (temp & 0x3F) | 0x80;
        }
        *len = *len + 1;
    }
    *(out++) = 0x00;
    return out;
}

time_t esp_mtp_utf16_datatime_to_time(const char *utf16)
{
    //"YYYYMMDDThhmmss.s",".s" 可选
    char buff[24];
    uint16_t *utf16_ptr = (uint16_t *)utf16;
    uint16_t *time_end = utf16_ptr + (sizeof("YYYYMMDDThhmmss") - 1);
    if (*time_end != 0 && *time_end != '.') {
        return 0;
    }
    char *data = buff;
    while (utf16_ptr < time_end) {
        if (*utf16_ptr > 0x7F) {
            return 0;
        }
        *(data++) = *(utf16_ptr++);
        uint32_t off;
        off = utf16_ptr - (uint16_t *)utf16;
        if (off == 4 || off == 6 || off == 11 || off == 13) {
            *(data++) = '-';
        }
    }
    *(data) = '\0';
    struct tm tm_time;
    if (strptime((char *)buff, "%Y-%m-%dT%H-%M-%S", &tm_time) != data) {
        return 0;
    }
    return mktime(&tm_time);
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
#if defined CONFIG_SPIRAM_USE_MALLOC || defined CONFIG_SPIRAM_USE_CAPS_ALLOC
            list->next = (esp_mtp_file_list_t *)heap_caps_calloc(1, sizeof(esp_mtp_file_list_t), MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
#else            
            list->next = (esp_mtp_file_list_t *)calloc(1, sizeof(esp_mtp_file_list_t));
#endif
            if (list->next == NULL) {
                return 0;
            }
        }
        list = list->next;
    }
#if defined CONFIG_SPIRAM_USE_MALLOC || defined CONFIG_SPIRAM_USE_CAPS_ALLOC
    list->entry_list[file_list->count % MTP_FILE_LIST_SIZE].name = (char *)heap_caps_malloc(strlen(name) + 1, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
#else
    list->entry_list[file_list->count % MTP_FILE_LIST_SIZE].name = (char *)malloc(strlen(name) + 1);
#endif
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

void esp_mtp_file_list_clean(esp_mtp_file_handle_list_t *file_list)
{
    esp_mtp_file_list_t *list;
    esp_mtp_file_list_t *next;

    list = &file_list->list;
    do {
        for (uint32_t i = 0; i < MTP_FILE_LIST_SIZE; i++) {
            if (list->entry_list[i].name) {
                free(list->entry_list[i].name);
            }
        }
        next = list->next;
        if (list != &file_list->list) {
            free(list);
        }
        list = next;
    } while (list != NULL);
    memset(&file_list->list, 0, sizeof(file_list->list));
    file_list->count = 0;
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