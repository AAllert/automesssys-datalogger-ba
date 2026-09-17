idf_component_register(
    SRCS "can_backend.c" "can_backend_usb.c" "can_backend_twai.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver utils usb_serial can app_config
)
