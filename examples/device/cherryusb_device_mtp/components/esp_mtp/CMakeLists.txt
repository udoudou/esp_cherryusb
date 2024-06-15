set(srcs "esp_mtp.c" "esp_mtp_helper.c")

if(CONFIG_CHERRYUSBD_ENABLED)
    list(APPEND srcs "usb_mtp.c")
endif()

idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "include/private"
    REQUIRES esp_cherryusb
    PRIV_REQUIRES fatfs
)
