set(cusb_path "CherryUSB")
set(srcs)
set(inc_dirs)

if(CONFIG_CHERRYUSB_SUPPORTED)
    list(APPEND srcs "additions/esp_cherryusb.c")
    list(APPEND inc_dirs "additions" "${cusb_path}/common" "${cusb_path}/core")
endif()

# USB Device sources
if(CONFIG_CHERRYUSBD_ENABLED)
    list(APPEND srcs
        "${cusb_path}/core/usbd_core.c"
        "${cusb_path}/port/dwc2/usb_dc_dwc2.c")

        list(APPEND inc_dirs "${cusb_path}/class/cdc")
        list(APPEND srcs "${cusb_path}/class/cdc/usbd_cdc.c")
        
        list(APPEND inc_dirs "${cusb_path}/class/hid")
        list(APPEND srcs "${cusb_path}/class/hid/usbd_hid.c")
        
        list(APPEND inc_dirs "${cusb_path}/class/msc")
        list(APPEND srcs "${cusb_path}/class/msc/usbd_msc.c")

        list(APPEND inc_dirs "${cusb_path}/class/audio")
        list(APPEND srcs "${cusb_path}/class/audio/usbd_audio.c")
    
        list(APPEND inc_dirs "${cusb_path}/class/video")
        list(APPEND srcs "${cusb_path}/class/video/usbd_video.c")

        list(APPEND inc_dirs "${cusb_path}/class/wireless")
        list(APPEND srcs "${cusb_path}/class/wireless/usbd_rndis.c")

        # list(APPEND inc_dirs "${cusb_path}/class/cdc")
        list(APPEND srcs "${cusb_path}/class/cdc/usbd_cdc_ecm.c")

        list(APPEND inc_dirs "${cusb_path}/class/dfu")
        list(APPEND srcs "${cusb_path}/class/dfu/usbd_dfu.c")
endif()

# USB Host sources
if(CONFIG_CHERRYUSBH_ENABLED)
    set(ldfragments "additions/linker.lf")

    list(APPEND srcs
        "${cusb_path}/core/usbh_core.c"
        "${cusb_path}/port/dwc2/usb_hc_dwc2.c"
        "additions/osal/usb_osal_idf.c")
    
    list(APPEND inc_dirs "${cusb_path}/class/hub")
    list(APPEND srcs "${cusb_path}/class/hub/usbh_hub.c")

    list(APPEND srcs "${cusb_path}/class/hub/usbh_hub.c")
    
    if(CONFIG_CHERRYUSBH_CDC_ACM_ENABLED)
        list(APPEND inc_dirs "${cusb_path}/class/cdc")
        list(APPEND srcs "${cusb_path}/class/cdc/usbh_cdc_acm.c")
    endif()
    
    if(CONFIG_CHERRYUSBH_HID_ENABLED)
        list(APPEND inc_dirs "${cusb_path}/class/hid")
        list(APPEND srcs "${cusb_path}/class/hid/usbh_hid.c")
    endif()

    if(CONFIG_CHERRYUSBH_MSC_ENABLED)
        list(APPEND inc_dirs "${cusb_path}/class/msc")
        list(APPEND srcs "${cusb_path}/class/msc/usbh_msc.c")
    endif()

    if(CONFIG_CHERRYUSBH_CDC_ECM_ENABLED)
        list(APPEND inc_dirs "${cusb_path}/class/cdc")
        list(APPEND srcs "${cusb_path}/class/cdc/usbh_cdc_ecm.c")
    endif()

    if(CONFIG_CHERRYUSBH_CDC_RNDIS_ENABLED)
        list(APPEND inc_dirs "${cusb_path}/class/wireless")
        list(APPEND srcs "${cusb_path}/class/wireless/usbh_rndis.c")
    endif()

    if(CONFIG_CHERRYUSBH_CDC_NCM_ENABLED)
        list(APPEND inc_dirs "${cusb_path}/class/cdc")
        list(APPEND srcs "${cusb_path}/class/cdc/usbh_cdc_ncm.c")
    endif()

    if(CONFIG_CHERRYUSBH_VIDEO_ENABLED)
        list(APPEND inc_dirs "${cusb_path}/class/video")
        list(APPEND srcs "${cusb_path}/class/video/usbh_video.c")
    endif()

    if(CONFIG_CHERRYUSBH_AUDIO_ENABLED)
        list(APPEND inc_dirs "${cusb_path}/class/audio")
        list(APPEND srcs "${cusb_path}/class/audio/usbh_audio.c")
    endif()

    if(CONFIG_CHERRYUSBH_BLUETOOTH_ENABLED)
        list(APPEND inc_dirs "${cusb_path}/class/wireless")
        list(APPEND srcs "${cusb_path}/class/wireless/usbh_bluetooth.c")
    endif()

    if(CONFIG_CHERRYUSBH_ASIX_ENABLED)
        list(APPEND inc_dirs "${cusb_path}/class/vendor/net")
        list(APPEND srcs "${cusb_path}/class/vendor/net/usbh_asix.c")
    endif()

    if(CONFIG_CHERRYUSBH_RTL8152_ENABLED)
        list(APPEND inc_dirs "${cusb_path}/class/vendor/net")
        list(APPEND srcs "${cusb_path}/class/vendor/net/usbh_rtl8152.c")
    endif()

    if(CONFIG_CHERRYUSBH_FTDI)
        list(APPEND inc_dirs "${cusb_path}/class/vendor/serial")
        list(APPEND srcs "${cusb_path}/class/vendor/serial/usbh_ftdi.c")
    endif()

    if(CONFIG_CHERRYUSBH_CH34X)
        list(APPEND inc_dirs "${cusb_path}/class/vendor/serial")
        list(APPEND srcs "${cusb_path}/class/vendor/serial/usbh_ch34x.c")
    endif()

    if(CONFIG_CHERRYUSBH_CP210X)
        list(APPEND inc_dirs "${cusb_path}/class/vendor/serial")
        list(APPEND srcs "${cusb_path}/class/vendor/serial/usbh_cp210x.c")
    endif()

    if(CONFIG_CHERRYUSBH_PL2303)
        list(APPEND inc_dirs "${cusb_path}/class/vendor/serial")
        list(APPEND srcs "${cusb_path}/class/vendor/serial/usbh_pl2303.c")
    endif()

endif()

idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS ${inc_dirs}
    PRIV_REQUIRES usb
    LDFRAGMENTS  ${ldfragments}
)

if(CONFIG_CHERRYUSBH_ENABLED)
    target_linker_script(${COMPONENT_LIB} INTERFACE "additions/usbh_class_info.ld")

    # 强制链接器不删除符号
    if(CONFIG_CHERRYUSBH_CDC_ACM_ENABLED)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u cdc_acm_class_info")
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u cdc_data_class_info")
    endif()
    if(CONFIG_CHERRYUSBH_HID_ENABLED)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u hid_custom_class_info")
    endif()
    if(CONFIG_CHERRYUSBH_MSC_ENABLED)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u msc_class_info")
    endif()
    if(CONFIG_CHERRYUSBH_CDC_ECM_ENABLED)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u cdc_ecm_class_info")
    endif()
    if(CONFIG_CHERRYUSBH_CDC_RNDIS_ENABLED)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u rndis_class_info")
    endif()
    if(CONFIG_CHERRYUSBH_CDC_NCM_ENABLED)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u cdc_ncm_class_info")
    endif()
    if(CONFIG_CHERRYUSBH_VIDEO_ENABLED)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u video_ctrl_class_info")
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u video_streaming_class_info")
    endif()
    if(CONFIG_CHERRYUSBH_AUDIO_ENABLED)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u audio_ctrl_intf_class_info")
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u audio_streaming_intf_class_info")
    endif()
    if(CONFIG_CHERRYUSBH_BLUETOOTH_ENABLED)
        if(CONFIG_USBHOST_BLUETOOTH_HCI_H4)
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u bluetooth_h4_nrf_class_info")
        else()
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u bluetooth_class_info")
        endif()
    endif()
    if(CONFIG_CHERRYUSBH_ASIX_ENABLED)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u asix_class_info")
    endif()
    if(CONFIG_CHERRYUSBH_RTL8152_ENABLED)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u rtl8152_class_info")
    endif()
    if(CONFIG_CHERRYUSBH_FTDI)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ftdi_class_info")
    endif()
    if(CONFIG_CHERRYUSBH_CH34X)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ch34x_class_info")
    endif()
    if(CONFIG_CHERRYUSBH_CP210X)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u cp210x_class_info")
    endif()
    if(CONFIG_CHERRYUSBH_PL2303)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-u pl2303_class_info")
    endif()
endif()

if(CONFIG_CHERRYUSB_SUPPORTED)
    set_source_files_properties("${cusb_path}/class/audio/usbd_audio.c"
        PROPERTIES COMPILE_FLAGS
        -Wno-maybe-uninitialized)
    include(package_manager)
    cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
endif()
