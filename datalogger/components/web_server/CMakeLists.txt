idf_component_register(
    SRCS "web_server.c" "rest_endpoints.c" "websocket.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_include"
    PRIV_REQUIRES
        app_config
        datalogger_service
        esp_event
        esp_http_server 
        esp_timer
        event_bus
        json
        log_manager
        mdns
        ota
        state_machine
        status_indicator
        storage
        uds_config
)