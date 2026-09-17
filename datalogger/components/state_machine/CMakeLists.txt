idf_component_register(
    SRCS "state_machine.c" "states.c"
    INCLUDE_DIRS "include"
    REQUIRES app_config
    PRIV_REQUIRES
        datalogger_service
        deepsleep_handler
        event_bus
        status_indicator
        storage
        uds_config
        web_server
        wifi_ap
        pir_sensor
        ota
        esp_timer 
        can_backend
)
