idf_component_register(
    SRCS "uds.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES can_backend isotp uds_decoder
)
