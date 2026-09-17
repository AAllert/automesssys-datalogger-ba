idf_component_register(
    SRCS "main_vecu.c"
    INCLUDE_DIRS "."
    REQUIRES
        can_backend
        state_machine
        event_bus
)

spiffs_create_partition_image(storage ../../spiffs_data FLASH_IN_PROJECT)