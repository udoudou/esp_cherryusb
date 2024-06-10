/*
 * Copyright (c) 2024, udoudou
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "usbd_core.h"

#define USB_MTP_CLASS 0x06

#define USB_MTP_SUB_CLASS 0x01U
#define USB_MTP_PROTOCOL  0x01U

 /*Length of template descriptor: 23 bytes*/
#define MTP_DESCRIPTOR_LEN (9 + 7 + 7 + 7)

// clang-format off
#define MTP_DESCRIPTOR_INIT(bFirstInterface, out_ep, in_ep, int_ep, wMaxPacketSize, str_idx) \
    /* Interface */                                                      \
    0x09,                          /* bLength */                         \
    USB_DESCRIPTOR_TYPE_INTERFACE, /* bDescriptorType */                 \
    bFirstInterface,               /* bInterfaceNumber */                \
    0x00,                          /* bAlternateSetting */               \
    0x03,                          /* bNumEndpoints */                   \
    USB_MTP_CLASS,                 /* bInterfaceClass */                 \
    USB_MTP_SUB_CLASS,             /* bInterfaceSubClass */              \
    USB_MTP_PROTOCOL,              /* bInterfaceProtocol */              \
    str_idx,                       /* iInterface */                      \
    0x07,                          /* bLength */                         \
    USB_DESCRIPTOR_TYPE_ENDPOINT,  /* bDescriptorType */                 \
    out_ep,                        /* bEndpointAddress */                \
    0x02,                          /* bmAttributes */                    \
    WBVAL(wMaxPacketSize),         /* wMaxPacketSize */                  \
    0x00,                          /* bInterval */                       \
    0x07,                          /* bLength */                         \
    USB_DESCRIPTOR_TYPE_ENDPOINT,  /* bDescriptorType */                 \
    in_ep,                         /* bEndpointAddress */                \
    0x02,                          /* bmAttributes */                    \
    WBVAL(wMaxPacketSize),         /* wMaxPacketSize */                  \
    0x00,                          /* bInterval */                       \
    0x07,                          /* bLength */                         \
    USB_DESCRIPTOR_TYPE_ENDPOINT,  /* bDescriptorType */                 \
    int_ep,                        /* bEndpointAddress */                \
    0x03,                          /* bmAttributes */                    \
    0x1c, 0x00,                    /* wMaxPacketSize */                  \
    0x06                           /* bInterval */
// clang-format on


struct usbd_interface *usbd_mtp_init_intf(struct usbd_interface *intf,
    const uint8_t out_ep,
    const uint8_t in_ep,
    const uint8_t int_ep);

void usbd_mtp_deinit(void);