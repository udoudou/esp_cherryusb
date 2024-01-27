/*
 * Copyright (c) 2024, udoudou
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_mtp.h"
#include "esp_mtp_def.h"
#include "esp_mtp_helper.h"

#include "string.h"
#include "dirent.h"
#include "sys/stat.h"
#include "unistd.h"
#include "fcntl.h"
#include "utime.h"

#include "esp_vfs_fat.h"
#include "esp_log.h"

static char *TAG = "esp_mtp";

#define ASYNC_READ_NOTIFY_BIT  BIT0
#define ASYNC_WRITE_NOTIFY_BIT  BIT1

typedef struct esp_mtp {
    void *pipe_context;
    int (*read)(void *pipe_context, uint8_t *buffer, int len);
    int (*write)(void *pipe_context, const uint8_t *buffer, int len);
    esp_mtp_flags_t flags;
    TaskHandle_t task_hdl;
    int async_read_len;
    esp_mtp_file_handle_list_t handle_list;
    uint32_t buffer_size;
    uint8_t buff[0];
}esp_mtp_t;

const mtp_operation_code_t supported_operation_codes[] = {
    MTP_OPERATION_GET_DEVICE_INFO,
    MTP_OPERATION_OPEN_SESSION,
    MTP_OPERATION_CLOSE_SESSION,
    MTP_OPERATION_GET_STORAGE_IDS,
    MTP_OPERATION_GET_STORAGE_INFO,
    MTP_OPERATION_GET_OBJECT_HANDLES,
    MTP_OPERATION_GET_OBJECT_INFO,
    MTP_OPERATION_GET_OBJECT,
    MTP_OPERATION_DELETE_OBJECT,
    MTP_OPERATION_SEND_OBJECT_INFO,
    MTP_OPERATION_SEND_OBJECT,
    MTP_OPERATION_GET_PARTIAL_OBJECT,

    // MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED,
    // MTP_OPERATION_GET_OBJECT_PROP_DESC,
    // MTP_OPERATION_GET_OBJECT_PROP_VALUE,
    // MTP_OPERATION_SET_OBJECT_PROP_VALUE
};

//todo 不必要
const mtp_operation_code_t supported_event_codes[] = {

};

static void check_usb_len_mps_and_send_end(esp_mtp_handle_t handle, uint32_t len)
{
    bool need_send_end = false;
    if (handle->flags & (ESP_MTP_FLAG_USB_FS | ESP_MTP_FLAG_USB_HS)) {
        if (handle->flags & ESP_MTP_FLAG_USB_FS) {
            if (len % 64 == 0) {
                need_send_end = true;
            }
        } else {
            if (len % 512 == 0) {
                need_send_end = true;
            }
        }
        if (need_send_end) {
            handle->write(handle->pipe_context, NULL, 0);
            if (handle->flags & ESP_MTP_FLAG_ASYNC_WRITE) {
                uint32_t notify_value;
                xTaskNotifyWait(0x0, ASYNC_WRITE_NOTIFY_BIT, &notify_value, portMAX_DELAY);
            }
        }
    }
}


static mtp_response_code_t open_session(esp_mtp_handle_t handle)
{
    return MTP_RESPONSE_OK;
}

static mtp_response_code_t get_thumb(esp_mtp_handle_t handle)
{
    return MTP_RESPONSE_OK;
}

static mtp_response_code_t get_device_info(esp_mtp_handle_t handle)
{
    mtp_container_t *container = (mtp_container_t *)handle->buff;
    uint8_t *data = container->data;
    container->type = MTP_CONTAINER_DATA;
    *(uint16_t *)data = MTP_STANDARD_VERSION;            // Standard Version
    data += 2;
    *(uint32_t *)data = MTP_VENDOR_EXTN_ID;        // MTP Vendor Extension ID
    data += 4;
    *(uint16_t *)data = MTP_VENDOR_EXTN_VERSION;   // MTP Version
    data += 2;
    *data = handle->buff + handle->buffer_size - data;
    data = (uint8_t *)esp_mtp_utf8_to_utf16(MTP_VENDOR_EXTENSIONDESC_CHAR, (char *)data + 1, data);    // MTP Extensions
    *(mtp_functional_mode_t *)data = MTP_FUNCTIONAL_STANDARD;            // Functional Mode
    data += sizeof(mtp_functional_mode_t);

    *(uint32_t *)data = sizeof(supported_operation_codes) / sizeof(supported_operation_codes[0]);
    data += 4;
    memcpy(data, supported_operation_codes, sizeof(supported_operation_codes));  // Operations Supported
    data += sizeof(supported_operation_codes);

    *(uint32_t *)data = sizeof(supported_event_codes) / sizeof(supported_event_codes[0]);
    data += 4;
    memcpy(data, supported_event_codes, sizeof(supported_event_codes));         // Events Supported
    data += sizeof(supported_event_codes);

    // Supported device properties
    *(uint32_t *)data = 0;
    data += 4;
    // *(uint32_t *)data = 2;
    // data += 4;
    // *(uint16_t *)data = MTP_DEV_PROP_BATTERY_LEVEL;
    // data += 2;
    // *(uint16_t *)data = MTP_DEV_PROP_DEVICE_FRIENDLY_NAME;
    // data += 2;

    // Supported formats
    *(uint32_t *)data = 0;
    data += 4;

    // Playback Formats
    // *(uint32_t *)data = 0;
    // data += 4;
    *(uint32_t *)data = 2;
    data += 4;
    *(mtp_object_format_code_t *)data = MTP_OBJECT_FORMAT_UNDEFINED;
    data += 2;
    *(mtp_object_format_code_t *)data = MTP_OBJECT_FORMAT_ASSOCIATION;
    data += sizeof(mtp_object_format_code_t);

    *data = handle->buff + handle->buffer_size - data;
    data = (uint8_t *)esp_mtp_utf8_to_utf16("Espressif", (char *)data + 1, data);    // Manufacturer

    *data = handle->buff + handle->buffer_size - data;
    data = (uint8_t *)esp_mtp_utf8_to_utf16("ESP32-S3", (char *)data + 1, data);    // Model

    *data = handle->buff + handle->buffer_size - data;
    data = (uint8_t *)esp_mtp_utf8_to_utf16("0.0.1", (char *)data + 1, data);    // Device Version

    *data = handle->buff + handle->buffer_size - data;
    data = (uint8_t *)esp_mtp_utf8_to_utf16("123456", (char *)data + 1, data);    // Serial Number

    container->len = data - handle->buff;
    handle->write(handle->pipe_context, handle->buff, container->len);
    if (handle->flags & ESP_MTP_FLAG_ASYNC_WRITE) {
        uint32_t notify_value;
        xTaskNotifyWait(0x0, ASYNC_WRITE_NOTIFY_BIT, &notify_value, portMAX_DELAY);
    }
    check_usb_len_mps_and_send_end(handle, container->len);

    return MTP_RESPONSE_OK;
}

static mtp_response_code_t get_storage_ids(esp_mtp_handle_t handle)
{
    mtp_container_t *container = (mtp_container_t *)handle->buff;
    uint8_t *data = container->data;
    container->type = MTP_CONTAINER_DATA;
    //Storage ID array
    *(uint32_t *)data = 1;
    data += 4;
    *(uint32_t *)data = 0x010001;
    data += 4;
    container->len = data - handle->buff;
    handle->write(handle->pipe_context, handle->buff, container->len);
    if (handle->flags & ESP_MTP_FLAG_ASYNC_WRITE) {
        uint32_t notify_value;
        xTaskNotifyWait(0x0, ASYNC_WRITE_NOTIFY_BIT, &notify_value, portMAX_DELAY);
    }
    // 长度不会达到 MPS，无需检查
    // check_usb_len_mps_and_send_end(handle, container->len);

    return MTP_RESPONSE_OK;
}

static mtp_response_code_t get_storage_info(esp_mtp_handle_t handle)
{
    mtp_container_t *container = (mtp_container_t *)handle->buff;
    uint8_t *data = container->data;
    // container->operation.get_storage_info.storage_id;

    container->type = MTP_CONTAINER_DATA;
    //StorageInfo dataset

    *(mtp_storage_type_t *)data = MTP_STORAGE_FIXED_RAM;   // Storage Type
    data += sizeof(mtp_storage_type_t);

    *(mtp_file_system_type_t *)data = MTP_FILE_SYSTEM_GENERIC_HIERARCH;    // Filesystem Type
    data += sizeof(mtp_file_system_type_t);

    *(mtp_access_cap_t *)data = MTP_ACCESS_CAP_RW;          // Access Capability
    data += sizeof(mtp_access_cap_t);

    uint64_t total_bytes = 0, out_free_bytes = 0;
    esp_vfs_fat_info("/sdcard", &total_bytes, &out_free_bytes);
    *(uint64_t *)data = total_bytes;                                   // Max Capacity
    data += sizeof(uint64_t);

    *(uint64_t *)data = out_free_bytes;                               // Free space in Bytes
    data += sizeof(uint64_t);

    *(uint32_t *)data = 0xFFFFFFFF;                               // Free Space In Objects
    data += sizeof(uint32_t);


    *data = handle->buff + handle->buffer_size - data;
    data = (uint8_t *)esp_mtp_utf8_to_utf16("test", (char *)data + 1, data);    // Storage Description

    *data = handle->buff + handle->buffer_size - data;
    data = (uint8_t *)esp_mtp_utf8_to_utf16("0", (char *)data + 1, data);    // Volume Identifier

    container->len = data - handle->buff;
    handle->write(handle->pipe_context, handle->buff, container->len);
    if (handle->flags & ESP_MTP_FLAG_ASYNC_WRITE) {
        uint32_t notify_value;
        xTaskNotifyWait(0x0, ASYNC_WRITE_NOTIFY_BIT, &notify_value, portMAX_DELAY);
    }
    check_usb_len_mps_and_send_end(handle, container->len);
    return MTP_RESPONSE_OK;
}

static mtp_response_code_t get_object_handles(esp_mtp_handle_t handle)
{
    mtp_container_t *container = (mtp_container_t *)handle->buff;
    uint8_t *data = container->data;
    uint32_t parent_handle;

    //todo
    // 0xFFFFFFFF 代表获取所有 storage 上的 object_handles
    // container->operation.get_object_handles.storage_id

    if (container->operation.get_object_handles.object_format_code != 0) {
        return MTP_RESPONSE_SPECIFICATION_BY_FORMAT_UNSUPPORTED;
    }

    parent_handle = container->operation.get_object_handles.parent_handle;
    // 0xFFFFFFFF 代表获取根目录下的 object_handles
    if (parent_handle == 0xFFFFFFFF) {
        parent_handle = 0;
    }

    container->type = MTP_CONTAINER_DATA;
    *(uint32_t *)container->data = handle->buff + handle->buffer_size - container->data - 4;  // Number of Object Handles
    //todo 暂未支持多个 storage
    data = esp_mtp_file_list_fill_handle_array(&handle->handle_list, parent_handle, container->data + 4, (uint32_t *)container->data); // Object Handles

    if (*(uint32_t *)container->data == 0) {
        strcpy((char *)container->data, "/sdcard");
        data = container->data + strlen((char *)container->data);
        if (parent_handle != 0) {
            if (esp_mtp_file_list_find(&handle->handle_list, parent_handle, (char *)data, handle->buff + handle->buffer_size - data) == NULL) {
                return MTP_RESPONSE_INVALID_PARENT_OBJECT;
            }
        }

        {
            DIR *dir;
            struct dirent *file;
            dir = opendir((char *)container->data);
            if (dir == NULL) {
                ESP_LOGW(TAG, "Failed to open dir for reading:%s", container->data);
                return MTP_RESPONSE_INVALID_PARENT_OBJECT;
            }
            data = container->data + sizeof(uint32_t);
            while ((file = readdir(dir)) != NULL) {
                *(uint32_t *)data = esp_mtp_file_list_add(&handle->handle_list, 0x00010001, parent_handle, file->d_name);
                if (*(uint32_t *)data == 0) {
                    ESP_LOGW(TAG, "add file list fail");
                    break;
                }
                data += sizeof(uint32_t);
            }
            closedir(dir);
            *(uint32_t *)container->data = (data - (container->data + sizeof(uint32_t))) / sizeof(uint32_t);
        }
    }

    container->len = data - handle->buff;
    handle->write(handle->pipe_context, handle->buff, container->len);
    if (handle->flags & ESP_MTP_FLAG_ASYNC_WRITE) {
        uint32_t notify_value;
        xTaskNotifyWait(0x0, ASYNC_WRITE_NOTIFY_BIT, &notify_value, portMAX_DELAY);
    }
    check_usb_len_mps_and_send_end(handle, container->len);
    return MTP_RESPONSE_OK;
}

static mtp_response_code_t get_object_info(esp_mtp_handle_t handle)
{
    uint8_t *data;
    uint32_t object_handle;
    const esp_mtp_file_entry_t *entry;
    mtp_container_t *container = (mtp_container_t *)handle->buff;

    object_handle = container->operation.get_object_info.object_handle;
    strcpy((char *)container->data, "/sdcard");
    data = container->data + strlen((char *)container->data);
    entry = esp_mtp_file_list_find(&handle->handle_list, object_handle, (char *)data, handle->buff + handle->buffer_size - data);
    if (entry == NULL) {
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    }
    ESP_LOGW(TAG, "%s %s", __FUNCTION__, (char *)container->data);
    struct stat st;
    if (stat((char *)container->data, &st) != 0) {
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    }

    container->type = MTP_CONTAINER_DATA;
    data = container->data;

    *(uint32_t *)data = entry->storage_id;  // StorageID
    *(uint32_t *)data = 0x010001;
    data += sizeof(uint32_t);

    if (S_ISDIR(st.st_mode)) {
        *(uint16_t *)data = MTP_OBJECT_FORMAT_ASSOCIATION;            // ObjectFormat Code
    } else {
        *(uint16_t *)data = MTP_OBJECT_FORMAT_UNDEFINED;            // ObjectFormat Code
    }
    data += sizeof(uint16_t);

    *(uint16_t *)data = 0x0000;            // Protection Status
    data += sizeof(uint16_t);

    *(uint32_t *)data = st.st_size;  // Object Compressed Size
    data += sizeof(uint32_t);

    *(uint16_t *)data = 0x0000;            // Thumb Format(未使用)
    data += sizeof(uint16_t);

    *(uint32_t *)data = 0x0000;            // Thumb Compressed Size(未使用)
    data += sizeof(uint32_t);

    *(uint32_t *)data = 0x0000;            // Thumb Pix Width(未使用)
    data += sizeof(uint32_t);

    *(uint32_t *)data = 0x0000;            // Thumb Pix Height(未使用)
    data += sizeof(uint32_t);

    *(uint32_t *)data = 0x0000;            // Image Pix Width(未使用)
    data += sizeof(uint32_t);

    *(uint32_t *)data = 0x0000;            // Image Pix Height(未使用)
    data += sizeof(uint32_t);

    *(uint32_t *)data = 0x0000;            // Image Bit Depth(未使用)
    data += sizeof(uint32_t);

    *(uint32_t *)data = entry->parent;            // Parent Object
    data += sizeof(uint32_t);

    if (S_ISDIR(st.st_mode)) {
        *(mtp_association_type_t *)data = MTP_ASSOCIATION_GENERIC_FOLDER;            // Association Type
    } else {
        *(mtp_association_type_t *)data = MTP_ASSOCIATION_UNDEFINED;            // Association Type
    }
    data += sizeof(mtp_association_type_t);

    *(uint32_t *)data = 0x0;            // Association Description
    data += sizeof(uint32_t);

    *(uint32_t *)data = 0x0;            // Sequence Number(未使用)
    data += sizeof(uint32_t);

    *data = handle->buff + handle->buffer_size - data;
    data = (uint8_t *)esp_mtp_utf8_to_utf16(entry->name, (char *)data + 1, data);    // Filename

    // Date Created "YYYYMMDDThhmmss.s"
    *data = handle->buff + handle->buffer_size - data;
    data = (uint8_t *)esp_mtp_time_to_utf16_datatime(st.st_ctime, (char *)data + 1, data);

    // Date Modified "YYYYMMDDThhmmss.s"
    *data = handle->buff + handle->buffer_size - data;
    data = (uint8_t *)esp_mtp_time_to_utf16_datatime(st.st_mtime, (char *)data + 1, data);

    // *data = handle->buff + handle->buffer_size - data;
    // data = (uint8_t*)esp_mtp_utf8_to_utf16("", (char*)data + 1, data);    // Keywords(未使用)
    *data = 0x0;
    data++;

    container->len = data - handle->buff;
    handle->write(handle->pipe_context, handle->buff, container->len);
    if (handle->flags & ESP_MTP_FLAG_ASYNC_WRITE) {
        uint32_t notify_value;
        xTaskNotifyWait(0x0, ASYNC_WRITE_NOTIFY_BIT, &notify_value, portMAX_DELAY);
    }
    check_usb_len_mps_and_send_end(handle, container->len);
    return MTP_RESPONSE_OK;
}

static mtp_response_code_t _get_object_common(esp_mtp_handle_t handle, uint32_t object_handle, uint32_t offset, uint32_t max_bytes, uint32_t *actual_bytes)
{
    uint8_t *data;
    mtp_container_t *container = (mtp_container_t *)handle->buff;

    strcpy((char *)container->data, "/sdcard");
    data = container->data + strlen((char *)container->data);
    if (esp_mtp_file_list_find(&handle->handle_list, object_handle, (char *)data, handle->buff + handle->buffer_size - data) == NULL) {
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    }
    ESP_LOGW(TAG, "%s %s", __FUNCTION__, (char *)container->data);

    int fd;
    struct stat st;
    if (stat((char *)container->data, &st) != 0) {
        return MTP_RESPONSE_ACCESS_DENIED;
    }
    fd = open((char *)container->data, O_RDONLY);
    if (fd < 0) {
        return MTP_RESPONSE_ACCESS_DENIED;
    }
    if (offset) {
        if (offset > st.st_size) {
            offset = st.st_size;
        }
        lseek(fd, offset, SEEK_SET);
    }

    uint32_t trans_id;
    trans_id = container->trans_id;

    uint32_t file_size;
    uint32_t max_len;
    uint32_t read_len;
    uint32_t last_write_len = 0;

    file_size = st.st_size - offset;
    if (file_size > max_bytes) {
        st.st_size = max_bytes;
        file_size = max_bytes;
    }

    container->type = MTP_CONTAINER_DATA;
    container->len = MTP_CONTAINER_HEAD_LEN + st.st_size;
    data = container->data;

    max_len = handle->buffer_size - MTP_CONTAINER_HEAD_LEN;
    max_len = max_len / 2;
    max_len = max_len & (~0x1ff);  //512对齐
    ESP_LOGD(TAG, "max_len:%"PRIu32, max_len);
    mtp_response_code_t res = MTP_RESPONSE_OK;

    if (file_size == 0) {
        handle->write(handle->pipe_context, handle->buff, MTP_CONTAINER_HEAD_LEN);
        last_write_len = MTP_CONTAINER_HEAD_LEN;
    }

    while (file_size) {
        read_len = file_size > max_len ? max_len : file_size;
        if (read(fd, data, read_len) != read_len) {
            ESP_LOGE(TAG, "file read error");
            last_write_len = 0;
            res = MTP_RESPONSE_INCOMPLETE_TRANSFER;
            break;
        }
        if (file_size != st.st_size) {
            if (handle->flags & ESP_MTP_FLAG_ASYNC_WRITE) {
                uint32_t notify_value;
                xTaskNotifyWait(0x0, ASYNC_WRITE_NOTIFY_BIT, &notify_value, portMAX_DELAY);
            }
        }
        file_size -= read_len;
        last_write_len = file_size > 0 ? read_len : MTP_CONTAINER_HEAD_LEN + read_len;

        if (data == container->data) {
            handle->write(handle->pipe_context, handle->buff, last_write_len);
            data = container->data + max_len;
        } else {
            handle->write(handle->pipe_context, handle->buff + max_len, last_write_len);
            memcpy(handle->buff, data + read_len - MTP_CONTAINER_HEAD_LEN, MTP_CONTAINER_HEAD_LEN);
            data = container->data;
        }
    }
    close(fd);
    if (handle->flags & ESP_MTP_FLAG_ASYNC_WRITE) {
        uint32_t notify_value;
        xTaskNotifyWait(0x0, ASYNC_WRITE_NOTIFY_BIT, &notify_value, portMAX_DELAY);
    }
    check_usb_len_mps_and_send_end(handle, last_write_len);
    if (actual_bytes) {
        *actual_bytes = st.st_size - file_size;
    }
    container->trans_id = trans_id;
    return res;
}

static mtp_response_code_t get_object(esp_mtp_handle_t handle)
{
#if 0
    uint8_t *data;
    uint32_t object_handle;
    mtp_container_t *container = (mtp_container_t *)handle->buff;
    object_handle = container->operation.get_object.object_handle;
    strcpy((char *)container->data, "/sdcard");
    data = container->data + strlen((char *)container->data);
    if (esp_mtp_file_list_find(&handle->handle_list, object_handle, (char *)data, handle->buff + handle->buffer_size - data) == NULL) {
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    }
    ESP_LOGW(TAG, "%s %s", __FUNCTION__, (char *)container->data);

    int fd;
    struct stat st;
    if (stat((char *)container->data, &st) != 0) {
        return MTP_RESPONSE_ACCESS_DENIED;
    }
    fd = open((char *)container->data, O_RDONLY);
    if (fd < 0) {
        return MTP_RESPONSE_ACCESS_DENIED;
    }

    uint32_t trans_id;
    trans_id = container->trans_id;

    uint32_t file_size;
    uint32_t max_len;
    uint32_t read_len;
    uint32_t last_write_len = 0;
    file_size = st.st_size;
    container->type = MTP_CONTAINER_DATA;
    container->len = MTP_CONTAINER_HEAD_LEN + st.st_size;
    data = container->data;

    max_len = handle->buffer_size - MTP_CONTAINER_HEAD_LEN;
    max_len = max_len / 2;
    max_len = max_len & (~0x1ff);  //512对齐
    ESP_LOGD(TAG, "max_len:%"PRIu32, max_len);
    mtp_response_code_t res = MTP_RESPONSE_OK;

    while (file_size) {
        read_len = file_size > max_len ? max_len : file_size;
        if (read(fd, data, read_len) != read_len) {
            ESP_LOGE(TAG, "file read error");
            last_write_len = 0;
            res = MTP_RESPONSE_INCOMPLETE_TRANSFER;
            break;
        }
        if (file_size != st.st_size) {
            if (handle->flags & ESP_MTP_FLAG_ASYNC_WRITE) {
                uint32_t notify_value;
                xTaskNotifyWait(0x0, ASYNC_WRITE_NOTIFY_BIT, &notify_value, portMAX_DELAY);
            }
        }
        file_size -= read_len;
        last_write_len = file_size > 0 ? read_len : MTP_CONTAINER_HEAD_LEN + read_len;

        if (data == container->data) {
            handle->write(handle->pipe_context, handle->buff, last_write_len);
            data = container->data + max_len;
        } else {
            handle->write(handle->pipe_context, handle->buff + max_len, last_write_len);
            memcpy(handle->buff, data + read_len - MTP_CONTAINER_HEAD_LEN, MTP_CONTAINER_HEAD_LEN);
            data = container->data;
        }
    }
    close(fd);
    if (handle->flags & ESP_MTP_FLAG_ASYNC_WRITE) {
        uint32_t notify_value;
        xTaskNotifyWait(0x0, ASYNC_WRITE_NOTIFY_BIT, &notify_value, portMAX_DELAY);
    }
    check_usb_len_mps_and_send_end(handle, last_write_len);
    container->trans_id = trans_id;
    return res;
#else
    uint32_t object_handle;
    mtp_container_t *container = (mtp_container_t *)handle->buff;
    object_handle = container->operation.get_object.object_handle;
    return _get_object_common(handle, object_handle, 0x0, 0xFFFFFFFF, NULL);
#endif
}

static mtp_response_code_t send_object_info(esp_mtp_handle_t handle)
{
    uint8_t *data;
    uint32_t parent_handle;
    uint32_t object_handle;
    uint32_t storage_id;
    mtp_container_t *container = (mtp_container_t *)handle->buff;

    storage_id = container->operation.send_object_info.storage_id;
    parent_handle = container->operation.send_object_info.parent_handle;
    if (parent_handle == 0xFFFFFFFF) {
        parent_handle = 0;
    }

    int len;
    len = handle->read(handle->pipe_context, handle->buff, handle->buffer_size);
    if (handle->flags & ESP_MTP_FLAG_ASYNC_READ) {
        uint32_t notify_value;
        do {
            xTaskNotifyWait(0x0, ASYNC_READ_NOTIFY_BIT | ASYNC_WRITE_NOTIFY_BIT, &notify_value, portMAX_DELAY);
        } while (!(notify_value & ASYNC_READ_NOTIFY_BIT));
        len = handle->async_read_len;
    }

    data = container->data;
    //Skip No Use StorageID
    data += sizeof(uint32_t);

    //ObjectFormat Code
    mtp_object_format_code_t object_format;
    object_format = *(mtp_object_format_code_t *)data;
    data += sizeof(mtp_object_format_code_t);

    //Skip No Use Protection Status
    data += sizeof(uint16_t);

    uint32_t file_size;
    file_size = *(uint32_t *)data;   //Object Compressed Size
    data += sizeof(uint32_t);

    //Skip No Use Thumb Format
    data += sizeof(uint16_t);

    //Skip No Use Thumb Compressed Size
    data += sizeof(uint32_t);

    //Skip No Use Thumb Pix Width
    data += sizeof(uint32_t);

    //Skip No Use Thumb Pix Height
    data += sizeof(uint32_t);

    //Skip No Use Image Pix Width
    data += sizeof(uint32_t);

    //Skip No Use Image Pix Height
    data += sizeof(uint32_t);

    //Skip No Use Image Bit Depth
    data += sizeof(uint32_t);

    //Skip No Use Parent Object
    data += sizeof(uint32_t);

    //Ignore Association Type
    data += sizeof(mtp_association_type_t);

    //Ignore Association Desc
    data += sizeof(uint32_t);

    //Skip No Use Sequence Number
    data += sizeof(uint32_t);

    //Filename
    uint8_t str_len;
    str_len = *data;
    data += sizeof(uint8_t);
    char filename[255];
    uint8_t temp_len = sizeof(filename);
    esp_mtp_utf16_to_utf8((char *)data, filename, &temp_len);
    ESP_LOGW(TAG, "%s %s", __FUNCTION__, filename);

    data += (str_len * 2);

    struct utimbuf times;
    //Date Created "YYYYMMDDThhmmss.s"
    str_len = *data;
    data += sizeof(uint8_t);
    times.actime = esp_mtp_utf16_datatime_to_time((char *)data);
    data += (str_len * 2);

    //Date Modified "YYYYMMDDThhmmss.s"
    str_len = *data;
    data += sizeof(uint8_t);
    times.modtime = esp_mtp_utf16_datatime_to_time((char *)data);
    data += (str_len * 2);

    //Skip No Use Keywords
    str_len = *data;
    data += sizeof(uint8_t);
    data += (str_len * 2);

    if (parent_handle == 0) {
        if (storage_id == 0) {
            return MTP_RESPONSE_INVALID_PARAMETER;
        }
    }
    //todo
    strcpy((char *)container->data, "/sdcard");
    data = container->data + strlen((char *)container->data);
    const esp_mtp_file_entry_t *entry = NULL;
    if (parent_handle != 0) {
        entry = esp_mtp_file_list_find(&handle->handle_list, parent_handle, (char *)data, handle->buff + handle->buffer_size - data);
        if (entry == NULL) {
            return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
        }
        data += strlen((char *)data);
    }

    if (handle->buff + handle->buffer_size - data < strlen(filename) + 2) {
        return MTP_RESPONSE_ACCESS_DENIED;
    }
    *data = '/';
    data++;
    strcpy((char *)data, filename);
    ESP_LOGW(TAG, "%s %s", __FUNCTION__, (char *)container->data);

    int fd = -1;
    if (object_format != MTP_OBJECT_FORMAT_ASSOCIATION) {
        fd = open((char *)container->data, O_WRONLY | O_CREAT | O_EXCL);
        if (fd < 0) {
            return MTP_RESPONSE_ACCESS_DENIED;
        }

    } else {
        if (mkdir((char *)container->data, 755) != 0) {
            return MTP_RESPONSE_ACCESS_DENIED;
        }
    }

    mtp_response_code_t req = MTP_RESPONSE_MAX;

    container->response.send_object_info.storage_id = storage_id;
    container->response.send_object_info.parent_handle = parent_handle;
    if (container->response.send_object_info.parent_handle == 0) {
        container->response.send_object_info.parent_handle = 0xFFFFFFFF;
    }
    object_handle = esp_mtp_file_list_add(&handle->handle_list, 0x00010001, parent_handle, filename);
    if (object_handle == 0) {
        ESP_LOGW(TAG, "add file list fail");
        req = MTP_RESPONSE_ACCESS_DENIED;
        goto exit;
    }
    container->response.send_object_info.object_handle = object_handle;
    container->len = MTP_CONTAINER_HEAD_LEN + 12;
    container->type = MTP_CONTAINER_RESPONSE;
    container->res = MTP_RESPONSE_OK;
    handle->write(handle->pipe_context, handle->buff, container->len);

    if (fd >= 0) {

        if (file_size) {
            uint32_t max_len;
            uint32_t read_len;
            uint32_t last_write_len = 0;

            max_len = handle->buffer_size - MTP_CONTAINER_HEAD_LEN;
            max_len = max_len / 2;
            max_len = max_len & (~0x1ff);  //512对齐

            len = handle->read(handle->pipe_context, handle->buff, max_len);
            if (handle->flags & ESP_MTP_FLAG_ASYNC_READ) {
                uint32_t notify_value;
                do {
                    xTaskNotifyWait(0x0, ASYNC_READ_NOTIFY_BIT | ASYNC_WRITE_NOTIFY_BIT, &notify_value, portMAX_DELAY);
                } while (!(notify_value & ASYNC_READ_NOTIFY_BIT));
                len = handle->async_read_len;
            }
            ESP_LOGI(TAG, "%d recv %d", __LINE__, len);

            if (container->type != MTP_CONTAINER_OPERATION || container->opt != MTP_OPERATION_SEND_OBJECT) {
                req = MTP_RESPONSE_PARAMETER_NOT_SUPPORTED;
                goto exit;
            }

            //DWC2 read len 为非 MPS 倍数时，如果主机发送大于 read len 的数据会产生错误
            //len = handle->read(handle->pipe_context, handle->buff, MTP_CONTAINER_HEAD_LEN + max_len);
            len = handle->read(handle->pipe_context, handle->buff, max_len);
            if (handle->flags & ESP_MTP_FLAG_ASYNC_READ) {
                uint32_t notify_value;
                do {
                    xTaskNotifyWait(0x0, ASYNC_READ_NOTIFY_BIT | ASYNC_WRITE_NOTIFY_BIT, &notify_value, portMAX_DELAY);
                } while (!(notify_value & ASYNC_READ_NOTIFY_BIT));
                len = handle->async_read_len;
            }
            ESP_LOGI(TAG, "%d recv %d", __LINE__, len);

            if (container->len != MTP_CONTAINER_HEAD_LEN + file_size || container->type != MTP_CONTAINER_DATA || container->opt != MTP_OPERATION_SEND_OBJECT) {
                req = MTP_RESPONSE_PARAMETER_NOT_SUPPORTED;
                goto exit;
            }
            last_write_len = len - MTP_CONTAINER_HEAD_LEN;
            data = container->data;
            while (1) {
                file_size -= last_write_len;
                if (file_size) {
                    read_len = file_size > max_len ? max_len : file_size;
                    if (data == container->data) {
                        len = handle->read(handle->pipe_context, container->data + max_len, max_len);
                    } else {
                        len = handle->read(handle->pipe_context, container->data, max_len);
                    }
                }
                if (write(fd, data, last_write_len) != last_write_len) {
                    req = MTP_RESPONSE_ACCESS_DENIED;
                    goto exit;
                }
                if (file_size == 0) {
                    break;
                }
                if (handle->flags & ESP_MTP_FLAG_ASYNC_READ) {
                    uint32_t notify_value;
                    do {
                        xTaskNotifyWait(0x0, ASYNC_READ_NOTIFY_BIT | ASYNC_WRITE_NOTIFY_BIT, &notify_value, portMAX_DELAY);
                    } while (!(notify_value & ASYNC_READ_NOTIFY_BIT));
                    len = handle->async_read_len;
                }
                if (len != read_len) {
                    req = MTP_RESPONSE_PARAMETER_NOT_SUPPORTED;
                    goto exit;
                }
                last_write_len = len;
                if (data == container->data) {
                    data = container->data + max_len;
                } else {
                    data = container->data;
                }
            }
            req = MTP_RESPONSE_OK;
            ESP_LOGI(TAG, "recv file ok");
        }
    }

exit:
    if (fd >= 0) {
        close(fd);
    }
    if (object_handle != 0) {
        //todo
        strcpy((char *)container->data, "/sdcard");
        data = container->data + strlen((char *)container->data);
        if (esp_mtp_file_list_find(&handle->handle_list, object_handle, (char *)data, handle->buff + handle->buffer_size - data) != NULL) {
            utime((char *)container->data, &times);
        }
    }
    return req;
}

static mtp_response_code_t send_object(esp_mtp_handle_t handle)
{
    int len;
    mtp_container_t *container = (mtp_container_t *)handle->buff;
    len = handle->read(handle->pipe_context, handle->buff, handle->buffer_size);
    if (handle->flags & ESP_MTP_FLAG_ASYNC_READ) {
        uint32_t notify_value;
        do {
            xTaskNotifyWait(0x0, ASYNC_READ_NOTIFY_BIT | ASYNC_WRITE_NOTIFY_BIT, &notify_value, portMAX_DELAY);
        } while (!(notify_value & ASYNC_READ_NOTIFY_BIT));
        len = handle->async_read_len;
    }
    // 有文件内容接收已在 send_object_info 中进行处理，此处仅处理无内容接收的包
    if (len != container->len || container->len != MTP_CONTAINER_HEAD_LEN || container->type != MTP_CONTAINER_DATA || container->opt != MTP_OPERATION_SEND_OBJECT) {
        return MTP_RESPONSE_PARAMETER_NOT_SUPPORTED;
    }
    return MTP_RESPONSE_OK;
}

static void delete_dir(char *path, uint32_t path_len, uint32_t max_len)
{
    DIR *dir;
    struct dirent *file;
    char *data;
    dir = opendir(path);
    if (dir == NULL) {
        ESP_LOGW(TAG, "Failed to open dir for reading:%s", path);
        return;
    }
    data = path + path_len;
    *data = '/';
    data++;
    while ((file = readdir(dir)) != NULL) {
        if (strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0) {
            continue;
        }
        uint32_t len = strlen(file->d_name);
        if (path_len + len + 2 > max_len) {
            ESP_LOGW(TAG, "path too long:%s", file->d_name);
            continue;
        }
        strcpy(data, file->d_name);
        if (file->d_type == DT_DIR) {
            delete_dir(path, path_len + len + 1, max_len);
        } else {
            unlink(path);
        }
    }
    data--;
    *data = '\0';
    closedir(dir);
    rmdir(path);
}

static mtp_response_code_t delete_object(esp_mtp_handle_t handle)
{
    uint8_t *data;
    uint32_t object_handle;
    mtp_container_t *container = (mtp_container_t *)handle->buff;

    object_handle = container->operation.delete_object.object_handle;
    if (object_handle == 0x0) {
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    }
    if (object_handle == 0xFFFFFFFF && container->operation.delete_object.object_format_code != 0x0) {
        return MTP_RESPONSE_SPECIFICATION_BY_FORMAT_UNSUPPORTED;
    }
    strcpy((char *)container->data, "/sdcard");
    data = container->data + strlen((char *)container->data);
    const esp_mtp_file_entry_t *entry = NULL;
    if (object_handle != 0) {
        entry = esp_mtp_file_list_find(&handle->handle_list, object_handle, (char *)data, handle->buff + handle->buffer_size - data);
        if (entry == NULL) {
            return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
        }
        data += strlen((char *)data);
    }

    struct stat st;
    if (stat((char *)container->data, &st) != 0) {
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    }
    if (S_ISDIR(st.st_mode)) {
        delete_dir((char *)container->data, data - container->data, handle->buffer_size - MTP_CONTAINER_HEAD_LEN);
    } else {
        unlink((char *)container->data);
    }

    return MTP_RESPONSE_OK;
}

static mtp_response_code_t get_partial_object(esp_mtp_handle_t handle)
{
    mtp_response_code_t res;
    mtp_container_t *container = (mtp_container_t *)handle->buff;
    uint32_t len = 0;
    res = _get_object_common(handle, container->operation.get_partial_object.object_handle, container->operation.get_partial_object.offset, container->operation.get_partial_object.max_bytes, &len);
    if (res != MTP_RESPONSE_OK) {
        return res;
    }
    container->response.get_partial_object.actual_bytes = len;
    container->len = MTP_CONTAINER_HEAD_LEN + 4;
    container->type = MTP_CONTAINER_RESPONSE;
    container->res = MTP_RESPONSE_OK;
    handle->write(handle->pipe_context, handle->buff, container->len);
    return MTP_RESPONSE_MAX;
}

static void esp_mtp_task(void *args)
{
    int len;
    esp_mtp_handle_t handle = (esp_mtp_handle_t)args;
    mtp_container_t *container;
    container = (mtp_container_t *)handle->buff;

wait:
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    while (1) {
        len = handle->read(handle->pipe_context, handle->buff, handle->buffer_size);
        if (handle->flags & ESP_MTP_FLAG_ASYNC_READ) {
            uint32_t notify_value;
            do {
                xTaskNotifyWait(0x0, ASYNC_READ_NOTIFY_BIT | ASYNC_WRITE_NOTIFY_BIT, &notify_value, portMAX_DELAY);
            } while (!(notify_value & ASYNC_READ_NOTIFY_BIT));
            len = handle->async_read_len;
        }
        if (len < 0) {
            break;
        }
        if (len == 0) {
            goto wait;
        }
        if (len != container->len || container->type != MTP_CONTAINER_OPERATION) {
            continue;
        }

        mtp_response_code_t res = MTP_RESPONSE_MAX;
        switch (container->opt) {
        case MTP_OPERATION_OPEN_SESSION:
            res = open_session(handle);
            break;
        case MTP_OPERATION_GET_THUMB:
            res = get_thumb(handle);
            break;
        case MTP_OPERATION_GET_DEVICE_INFO:
            res = get_device_info(handle);
            break;
        case MTP_OPERATION_GET_STORAGE_IDS:
            res = get_storage_ids(handle);
            break;
        case MTP_OPERATION_GET_STORAGE_INFO:
            res = get_storage_info(handle);
            break;
        case MTP_OPERATION_GET_OBJECT_HANDLES:
            res = get_object_handles(handle);
            break;
        case MTP_OPERATION_GET_OBJECT_INFO:
            res = get_object_info(handle);
            break;
        case MTP_OPERATION_GET_OBJECT:
            res = get_object(handle);
            break;
        case MTP_OPERATION_SEND_OBJECT_INFO:
            res = send_object_info(handle);
            break;
        case MTP_OPERATION_SEND_OBJECT:
            res = send_object(handle);
            break;
        case MTP_OPERATION_DELETE_OBJECT:
            res = delete_object(handle);
            break;
        case MTP_OPERATION_GET_PARTIAL_OBJECT:
            res = get_partial_object(handle);
            break;
        default:
            ESP_LOGW(TAG, "Undefine handle 0x%"PRIx16"(len:%"PRIu32")", container->opt, container->len);
            res = MTP_RESPONSE_OPERATION_NOT_SUPPORTED;
            break;
        }
        if (res != MTP_RESPONSE_MAX) {
            container->len = MTP_CONTAINER_HEAD_LEN;
            container->type = MTP_CONTAINER_RESPONSE;
            container->res = res;
            handle->write(handle->pipe_context, handle->buff, container->len);
            // 长度不会达到 MPS，无需检查
            // check_usb_len_mps_and_send_end(handle, container->len);
        }

    }
    ESP_LOGW(TAG, "MTP task exit");
    free(handle);
    vTaskDelete(NULL);
}

esp_mtp_handle_t esp_mtp_init(const esp_mtp_config_t *config)
{
    esp_mtp_handle_t handle;
    uint32_t buffer_size;
    buffer_size = config->buffer_size;
    if (buffer_size < 1024) {
        buffer_size = 1024;
    }

    handle = malloc(sizeof(esp_mtp_t) + MTP_CONTAINER_HEAD_LEN + buffer_size);

    esp_mtp_file_list_init(&handle->handle_list);

    handle->pipe_context = config->pipe_context;
    handle->read = config->read;
    handle->write = config->write;
    handle->flags = config->flags;
    handle->buffer_size = MTP_CONTAINER_HEAD_LEN + buffer_size;
    if (xTaskCreate(esp_mtp_task, "esp_mtp_task", 4096,
        handle, 5, &handle->task_hdl) != pdTRUE) {
        goto _exit;
    }
    return handle;
_exit:
    free(handle);
    return NULL;
}

void esp_mtp_read_async_cb(esp_mtp_handle_t handle, int len)
{
    BaseType_t high_task_wakeup;
    high_task_wakeup = pdFALSE;
    handle->async_read_len = len;
    xTaskNotifyFromISR(handle->task_hdl, ASYNC_READ_NOTIFY_BIT, eSetBits, &high_task_wakeup);
    if (high_task_wakeup == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

void esp_mtp_write_async_cb(esp_mtp_handle_t handle, int len)
{
    BaseType_t high_task_wakeup = pdFALSE;
    xTaskNotifyFromISR(handle->task_hdl, ASYNC_WRITE_NOTIFY_BIT, eSetBits, &high_task_wakeup);
    if (high_task_wakeup == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

TaskHandle_t esp_mtp_get_task_handle(esp_mtp_handle_t handle)
{
    return handle->task_hdl;
}