idf_component_register(
    SRCS "deepsleep_handler.c"
    INCLUDE_DIRS "include"
    REQUIRES driver 
    PRIV_REQUIRES app_config event_bus status_indicator storage
)