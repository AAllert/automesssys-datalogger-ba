idf_component_register(
    SRCS "datalogger_esp_test.c"
    INCLUDE_DIRS "."
    REQUIRES 
        driver 
        can_backend
        datalogger_service
        deepsleep_handler
        mdns 
        isotp
        usb_serial
        uds
        uds_config
        storage
        log_manager
        event_bus
        app_config
    PRIV_REQUIRES unity app_trace
)

spiffs_create_partition_image(storage ../../spiffs_data FLASH_IN_PROJECT)