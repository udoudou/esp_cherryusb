/*
 * Copyright (c) 2024, udoudou
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

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

uint8_t *esp_mtp_utf8_to_utf16(const char *utf8, uint8_t *out, uint8_t *len);

uint8_t *esp_mtp_utf16_to_utf8(const char *utf16, uint8_t *out, uint8_t *len);

void esp_mtp_file_list_init(esp_mtp_file_handle_list_t *file_list);

uint32_t esp_mtp_file_list_add(esp_mtp_file_handle_list_t *file_list, uint32_t storage_id, uint32_t parent, const char *name);

const esp_mtp_file_entry_t *esp_mtp_file_list_find(esp_mtp_file_handle_list_t *file_list, uint32_t handle, char *path, uint32_t max_len);

void esp_mtp_file_list_clean(esp_mtp_file_handle_list_t *file_list);

uint8_t *esp_mtp_file_list_fill_handle_array(esp_mtp_file_handle_list_t *file_list, uint32_t parent, uint8_t *out, uint32_t *len);