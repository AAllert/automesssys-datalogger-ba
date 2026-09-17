idf_component_register(
    SRCS "nvs.c" "sd_card.c" "spiffs.c" "storage.c"
    INCLUDE_DIRS "include"
    REQUIRES driver
    PRIV_REQUIRES spiffs fatfs sdmmc driver nvs_flash
)
