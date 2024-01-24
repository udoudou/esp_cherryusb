/*
 * Copyright (c) 2024, udoudou
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"

typedef struct esp_mtp *esp_mtp_handle_t;

typedef enum {
    ESP_MTP_FLAG_NONE = 0,
    ESP_MTP_FLAG_USB_FS = 1 << 0,
    ESP_MTP_FLAG_USB_HS = 1 << 1,
    ESP_MTP_FLAG_ASYNC_READ = 1 << 2,
    ESP_MTP_FLAG_ASYNC_WRITE = 1 << 3,
    ESP_MTP_FLAG_MAX = 0xFFFFFFFF,
} __attribute__((packed)) esp_mtp_flags_t;

typedef struct {
    void *pipe_context;
    int (*read)(void *pipe_context, uint8_t *buffer, int len);
    int (*write)(void *pipe_context, const uint8_t *buffer, int len);
    esp_mtp_flags_t flags;
    uint32_t buffer_size;
}esp_mtp_config_t;

esp_mtp_handle_t esp_mtp_init(const esp_mtp_config_t *config);

void esp_mtp_read_async_cb(esp_mtp_handle_t handle, int len);

void esp_mtp_write_async_cb(esp_mtp_handle_t handle, int len);

TaskHandle_t esp_mtp_get_task_handle(esp_mtp_handle_t handle);
