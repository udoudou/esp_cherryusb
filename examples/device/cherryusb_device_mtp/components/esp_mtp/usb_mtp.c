/*
 * Copyright (c) 2024, udoudou
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usb_mtp.h"
#include "esp_mtp_def.h"
#include "esp_mtp.h"

 /* Max USB packet size */
#ifndef CONFIG_USB_HS
#define MTP_BULK_EP_MPS 64
#else
#define MTP_BULK_EP_MPS 512
#endif

#define MTP_OUT_EP_IDX 0
#define MTP_IN_EP_IDX  1
#define MTP_INT_EP_IDX 2

typedef enum {
    USB_MTP_CLOSE,
    USB_MTP_INIT,
    USB_MTP_RUN,
    USB_MTP_STOPPING,
} usb_mtp_status_t;

/* Describe EndPoints configuration */
static struct usbd_endpoint mtp_ep_data[3];
static TaskHandle_t s_mtp_task_handle;
esp_mtp_handle_t s_handle;
static usb_mtp_status_t s_mtp_status = USB_MTP_CLOSE;
static portMUX_TYPE s_spinlock = portMUX_INITIALIZER_UNLOCKED;

static int mtp_class_interface_request_handler(uint8_t busid, struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("MTP Class request: "
        "bRequest 0x%02x\r\n",
        setup->bRequest);

    switch (setup->bRequest) {
    case MTP_REQUEST_CANCEL:

        break;
    case MTP_REQUEST_GET_EXT_EVENT_DATA:

        break;
    case MTP_REQUEST_RESET:

        break;
    case MTP_REQUEST_GET_DEVICE_STATUS:
        *(uint16_t *)(*data) = 0x08;
        *(mtp_response_code_t *)(*data + 2) = MTP_RESPONSE_OK;
        *len = 8;
        break;
    default:
        USB_LOG_WRN("Unhandled MTP Class bRequest 0x%02x\r\n", setup->bRequest);
        return -1;
    }

    return 0;
}

static void usbd_mtp_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    esp_mtp_read_async_cb(s_handle, nbytes);
}

static void usbd_mtp_bulk_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    esp_mtp_write_async_cb(s_handle, nbytes);
}

static void mtp_notify_handler(uint8_t busid, uint8_t event, void *arg)
{
    BaseType_t high_task_wakeup = pdFALSE;
    switch (event) {
    case USBD_EVENT_RESET:
        portENTER_CRITICAL_ISR(&s_spinlock);
        if (s_mtp_status != USB_MTP_CLOSE && s_mtp_status != USB_MTP_INIT) {
            s_mtp_status = USB_MTP_STOPPING;
        }
        portEXIT_CRITICAL_ISR(&s_spinlock);
        break;
    case USBD_EVENT_CONFIGURED:
        bool need_wake = false;
        portENTER_CRITICAL_ISR(&s_spinlock);
        if (s_mtp_status == USB_MTP_INIT) {
            need_wake = true;
            s_mtp_status = USB_MTP_RUN;
        }
        portEXIT_CRITICAL_ISR(&s_spinlock);
        if (need_wake) {
            vTaskNotifyGiveFromISR(s_mtp_task_handle, &high_task_wakeup);
        }
        break;
    case USBD_EVENT_DISCONNECTED:
        vTaskNotifyGiveFromISR(s_mtp_task_handle, &high_task_wakeup);
        break;
    default:
        break;
    }
    if (high_task_wakeup == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void usb_wait_start(void *pipe_context)
{
    portENTER_CRITICAL(&s_spinlock);
    if (s_mtp_status != USB_MTP_STOPPING) {
        portEXIT_CRITICAL(&s_spinlock);
        return;
    }
    s_mtp_status = USB_MTP_INIT;
    portEXIT_CRITICAL(&s_spinlock);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

static int usb_write(void *pipe_context, const uint8_t *data, int data_size)
{
    if (s_mtp_status != USB_MTP_RUN) {
        data_size = (s_mtp_status != USB_MTP_CLOSE) ? ESP_MTP_STOP_CMD : ESP_MTP_EXIT_CMD;
        esp_mtp_write_async_cb(s_handle, data_size);
        return data_size;
    }
    usbd_ep_start_write(0, mtp_ep_data[MTP_IN_EP_IDX].ep_addr, data, data_size);
    return data_size;
}

static int usb_read(void *pipe_context, uint8_t *data, int data_size)
{
    if (s_mtp_status != USB_MTP_RUN) {
        data_size = (s_mtp_status != USB_MTP_CLOSE) ? ESP_MTP_STOP_CMD : ESP_MTP_EXIT_CMD;
        esp_mtp_read_async_cb(s_handle, data_size);
        return data_size;
    }
    usbd_ep_start_read(0, mtp_ep_data[MTP_OUT_EP_IDX].ep_addr, data, data_size);
    return data_size;
}

struct usbd_interface *usbd_mtp_init_intf(struct usbd_interface *intf,
    const uint8_t out_ep,
    const uint8_t in_ep,
    const uint8_t int_ep)
{
    intf->class_interface_handler = mtp_class_interface_request_handler;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = mtp_notify_handler;

    mtp_ep_data[MTP_OUT_EP_IDX].ep_addr = out_ep;
    mtp_ep_data[MTP_OUT_EP_IDX].ep_cb = usbd_mtp_bulk_out;
    mtp_ep_data[MTP_IN_EP_IDX].ep_addr = in_ep;
    mtp_ep_data[MTP_IN_EP_IDX].ep_cb = usbd_mtp_bulk_in;

    //EVENT 通道
    mtp_ep_data[MTP_INT_EP_IDX].ep_addr = int_ep;
    mtp_ep_data[MTP_INT_EP_IDX].ep_cb = NULL;

    usbd_add_endpoint(0, &mtp_ep_data[MTP_OUT_EP_IDX]);
    usbd_add_endpoint(0, &mtp_ep_data[MTP_IN_EP_IDX]);
    usbd_add_endpoint(0, &mtp_ep_data[MTP_INT_EP_IDX]);

    s_mtp_status = USB_MTP_STOPPING;

    esp_mtp_config_t config = {
        .wait_start = usb_wait_start,
        .read = usb_read,
        .write = usb_write,
        .flags = ESP_MTP_FLAG_ASYNC_READ | ESP_MTP_FLAG_ASYNC_WRITE,
        .buffer_size = 4096,
    };
#ifndef CONFIG_USB_HS
    config.flags |= ESP_MTP_FLAG_USB_FS;
#else
    config.flags |= ESP_MTP_FLAG_USB_HS;
#endif

    s_handle = esp_mtp_init(&config);

    s_mtp_task_handle = esp_mtp_get_task_handle(s_handle);

    return intf;
}

void usbd_mtp_deinit(void)
{
    usb_mtp_status_t mtp_status;
    portENTER_CRITICAL(&s_spinlock);
    mtp_status = s_mtp_status;
    s_mtp_status = USB_MTP_CLOSE;
    portEXIT_CRITICAL(&s_spinlock);
    if (mtp_status == USB_MTP_RUN) {
        esp_mtp_read_async_cb(s_handle, ESP_MTP_EXIT_CMD);
    } else if (mtp_status == USB_MTP_INIT) {
        xTaskNotifyGive(s_mtp_task_handle);
    }
}