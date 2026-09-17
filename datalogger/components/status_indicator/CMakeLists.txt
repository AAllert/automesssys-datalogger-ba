idf_component_register(
    SRCS "status_indicator_leds.c"
    INCLUDE_DIRS "."
    REQUIRES app_config driver
    PRIV_REQUIRES esp_timer event_bus
)
