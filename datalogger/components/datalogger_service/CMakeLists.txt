idf_component_register(
    SRCS "datalogger_service.c"
    INCLUDE_DIRS "include"
    REQUIRES app_config uds uds_config
    PRIV_REQUIRES event_bus log_manager uds_decoder storage
)
