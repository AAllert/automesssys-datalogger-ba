idf_component_register(
    SRCS "log_manager.c" "csv_log_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES storage uds uds_config
    PRIV_REQUIRES uds_decoder
)
