idf_component_register(
    SRCS "pir_sensor.c"
    INCLUDE_DIRS "include"
    REQUIRES driver
    PRIV_REQUIRES app_config event_bus
)
