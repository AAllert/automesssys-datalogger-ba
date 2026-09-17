idf_component_register(
    SRCS "uds_config.c" "config_parser.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_include"
    REQUIRES storage
)
