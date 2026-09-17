idf_component_register(
    SRCS "event_loop.c"
    INCLUDE_DIRS "."
    REQUIRES esp_event app_config
)
