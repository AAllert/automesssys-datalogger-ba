// implements
#include "state_machine.h"

// system includes
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

// project includes
#include "event_loop.h"
#include "deepsleep_handler.h"
#include "status_indicator.h"
#include "datalogger_service.h"
#include "uds_config.h"
#include "can_backend.h"
#include "storage.h"
#include "nvs_storage.h"
#include "wifi_ap.h"
#include "web_server.h"
#include "pir_sensor.h"
#include "ota.h"

#define TAG "state_machine"

extern state_t state_deepsleep;
extern state_t state_initilizing;
extern state_t state_wait_term_15;
extern state_t state_undefined_error;
extern state_t state_parsing_config_error;
extern state_t state_log_error;
extern state_t state_can_error;
extern state_t state_memory_full;
extern state_t state_import_config;
extern state_t state_logging_active;
extern state_t state_logger_idle;

static esp_timer_handle_t term15_sleep_timer = NULL;
static bool term15_wifi_connected = false;
static uint32_t term15_sleep_timeout_s = DEFAULT_SLEEP_TIMEOUT_S;
static uint32_t term15_request_interval_ms = DEFAULT_TERM15_REQUEST_INTERVAL_MS;

static TaskHandle_t term15_task_handle = NULL;

#define CHECK(x) do {          \
    result = (x);              \
    if (result != ESP_OK) {    \
        goto end;              \
    }                          \
} while (0)

static void set_time_to_compile_time(void)
{
    struct tm tm = {0};

    strptime(__DATE__ " " __TIME__, "%b %d %Y %H:%M:%S", &tm);

    time_t t = mktime(&tm);

    struct timeval now = {
        .tv_sec = t,
        .tv_usec = 0
    };

    settimeofday(&now, NULL);
}

state_t* initilizing_enter(void) {
    esp_err_t result = ESP_OK;

    time_t now;
    time(&now);
    if (now < 1700000000) set_time_to_compile_time();

    CHECK(deepsleep_handler_init(PIN_NUM_MOSFET, PIN_NUM_PIR));
    deepsleep_activate_peripherals();
    CHECK(status_indicator_init(PIN_NUM_LED_RED, PIN_NUM_LED_YELLOW, PIN_NUM_LED_GREEN));
    CHECK(pir_sensor_init(PIN_NUM_PIR));
    CHECK(sdcard_init(PIN_NUM_CLK, PIN_NUM_CS, PIN_NUM_MISO, PIN_NUM_MOSI));
    CHECK(spiffs_init(MAX_SPIFFS_FILES));
    CHECK(nvs_init());
    CHECK(nvs_get_deepsleep_timeout(&term15_sleep_timeout_s));
    CHECK(nvs_get_term15_request_interval(&term15_request_interval_ms));
    CHECK(can_backend_init());
    CHECK(wifi_ap_init());
    CHECK(wifi_ap_start());
    CHECK(web_server_start());
    // Reaching this point means the current firmware booted and initilized successfully, confirm it so the rollback API allows starting further ota updates.
    CHECK(ota_confirm());

end:
    return (result == ESP_OK) ? &state_import_config : &state_undefined_error;
}

state_t* initilizing_handler(state_event_id_t event) {
    return &state_initilizing;
}

state_t state_initilizing = {
    .state_id = STATE_INITILIZING,
    .name = "initilizing",
    .handler = initilizing_handler,
    .on_enter = initilizing_enter
};

static void term15_sleep_timer_cb(void *arg) {
    event_loop_publish_state_event(STATE_EVENT_TIMEOUT_REACHED, NULL, 0);
}

static void start_sleep_timer(void) {
    if (term15_sleep_timer == NULL) {
        const esp_timer_create_args_t args = {
            .callback = term15_sleep_timer_cb,
            .name = "term15_sleep"
        };
        ESP_ERROR_CHECK(esp_timer_create(&args, &term15_sleep_timer));
    }
    esp_timer_stop(term15_sleep_timer);
    esp_timer_start_once(term15_sleep_timer, (uint64_t)term15_sleep_timeout_s * 1000000);
}

static void stop_sleep_timer(void) {
    if (term15_sleep_timer != NULL) esp_timer_stop(term15_sleep_timer);
}

void set_sleep_timeout_s(uint32_t timeout_s) {
    term15_sleep_timeout_s = timeout_s;
}

void set_term15_request_interval_ms(uint32_t interval_ms) {
    term15_request_interval_ms = interval_ms;
}

static void term15_request_task(void *arg) {
    uint32_t timeout_ms = ((uint32_t *)arg)[0];
    uint32_t interval_ms = ((uint32_t *)arg)[1];
    free(arg);
    uint32_t num_requests = timeout_ms / interval_ms;

    ConfigRow ignigtion_row;
    get_active_ignition_row(&ignigtion_row);

    uint32_t can_timeout;
    uint32_t uds_timeout;
    nvs_get_can_timeout(&can_timeout);
    nvs_get_uds_timeout(&uds_timeout);

    for (int i=0;i<num_requests;i++) {
        UdsResponse response = {0};
        bool result = uds_check_active_ignition(ignigtion_row, uds_timeout, can_timeout, &response);
        if (result) {
            ESP_LOGI(TAG, "Status Term15: On");
            event_loop_publish_state_event(STATE_EVENT_TERM_15_ON, NULL, 0);
            goto cleanup;
        } else {
            ESP_LOGI(TAG, "Status Term15: Off");
        }
        vTaskDelay(pdMS_TO_TICKS(interval_ms));
    }
    event_loop_publish_state_event(STATE_EVENT_TIMEOUT_REACHED, NULL, 0);

cleanup: 
    term15_task_handle = NULL;
    vTaskDelete(NULL);
}

static void start_term15_request_loop(uint32_t timeout_ms, uint32_t interval_ms) {
    if (term15_task_handle != NULL) return;
    uint32_t *params = malloc(2 * sizeof(uint32_t));
    params[0] = timeout_ms;
    params[1] = interval_ms;
    xTaskCreate(term15_request_task, "term15", 4096, params, 7, &term15_task_handle);
}

static void stop_term15_request_loop() {
    if (term15_task_handle != NULL) {
        vTaskDelete(term15_task_handle);
        term15_task_handle = NULL;
    }
}

state_t* wait_term15_enter(void) {
    term15_wifi_connected = wifi_ap_is_client_connected();
    start_term15_request_loop(term15_sleep_timeout_s * 1000, term15_request_interval_ms);
    if (term15_wifi_connected) {
        stop_sleep_timer();
    } else {
        start_sleep_timer();
    }
    return NULL;
}

state_t* wait_term15_handler(state_event_id_t event) {
    switch (event)
    {
    case STATE_EVENT_PIR_TRIGGERED:
        if (!term15_wifi_connected) {
            start_sleep_timer();
        }
        break;
    case STATE_EVENT_WIFI_CONNECTED:
        term15_wifi_connected = true;
        stop_sleep_timer();
        break;
    case STATE_EVENT_WIFI_DISCONNECTED:
        term15_wifi_connected = false;
        start_sleep_timer();
        break;
    case STATE_EVENT_TIMEOUT_REACHED:
        if (!term15_wifi_connected) {
            return &state_deepsleep;
        }
        start_term15_request_loop(term15_sleep_timeout_s * 1000, term15_request_interval_ms);
        break;
    case STATE_EVENT_TERM_15_ON:
        stop_sleep_timer();
        stop_term15_request_loop();
        ESP_LOGI(TAG, "Got Term15 on event");
        return &state_logging_active;
    case STATE_EVENT_WIFI_DATALOGGER_STOP:
        stop_sleep_timer();
        stop_term15_request_loop();
        return &state_logger_idle;
    default:
        break;
    }
    return &state_wait_term_15;
}

state_t state_wait_term_15 = {
    .state_id = STATE_WAIT_TERM15,
    .name = "wait_term15",
    .handler = wait_term15_handler,
    .on_enter = wait_term15_enter
};

state_t* error_enter(void) {
    stop_datalogger_service();
    if (!term15_wifi_connected) {
        start_sleep_timer();
    }
    return NULL;
}

state_t* error_handler(state_event_id_t event) {
    switch (event)
    {
    case STATE_EVENT_WIFI_DATALOGGER_START:
        return &state_wait_term_15;
    case STATE_EVENT_WIFI_DISCONNECTED:
        term15_wifi_connected = false;
        start_sleep_timer();
        break;
    case STATE_EVENT_TIMEOUT_REACHED:
        if (!term15_wifi_connected) {
            return &state_deepsleep;
        }
        break;
    case STATE_EVENT_WIFI_CONNECTED:
        term15_wifi_connected = true;
        stop_sleep_timer();
        break;
    default:
        break;
    }
    return state_machine_get_state();
}

state_t state_undefined_error = {
    .state_id = STATE_UNDEFINED_ERROR,
    .name = "undefined_error",
    .handler = error_handler,
    .on_enter = error_enter
};

state_t state_parsing_config_error = {
    .state_id = STATE_PARSING_CONFIG_ERROR,
    .name = "parsing_config_error",
    .handler = error_handler,
    .on_enter = error_enter
};

state_t state_log_error = {
    .state_id = STATE_LOG_ERROR,
    .name = "log_error",
    .handler = error_handler,
    .on_enter = error_enter
};

state_t state_can_error = {
    .state_id = STATE_CAN_ERROR,
    .name = "can_error",
    .handler = error_handler,
    .on_enter = error_enter
};

state_t state_memory_full = {
    .state_id = STATE_MEMORY_FULL,
    .name = "memory_full",
    .handler = error_handler,
    .on_enter = error_enter
};

state_t* import_config_enter(void) {
    esp_err_t err = parse_active_config();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "parse config OK");
        event_loop_publish_state_event(STATE_EVENT_CONFIG_LOADED, NULL, 0);
        return &state_wait_term_15;
    } else {
        event_loop_publish_state_event(STATE_EVENT_CONFIG_LOADING_FAILED, NULL, 0);
        ESP_LOGI(TAG, "parse config Failed");
        return &state_parsing_config_error;
    }
}

state_t* import_config_handler(state_event_id_t event) {
    switch (event)
    {
    case STATE_EVENT_CONFIG_LOADED:
        return &state_wait_term_15;
    case STATE_EVENT_CONFIG_LOADING_FAILED:
    ESP_LOGI(TAG, "go to config error");
        return &state_parsing_config_error;
    case STATE_EVENT_WIFI_DATALOGGER_STOP:
        return &state_logger_idle;
    default:
        break;
    }
    return &state_import_config;
}

state_t state_import_config = {
    .state_id = STATE_IMPORT_CONFIG,
    .name = "import_config",
    .handler = import_config_handler,
    .on_enter = import_config_enter
};

state_t* logging_active_enter(void) {
    uint32_t uds_timeout;
    uint32_t can_timeout;
    nvs_get_uds_timeout(&uds_timeout);
    nvs_get_can_timeout(&can_timeout);

    UdsConfig config = {0};
    esp_err_t error = get_active_config(&config);
    if (error != ESP_OK) return &state_parsing_config_error;

    error = start_datalogger_service(&config, uds_timeout, can_timeout);

    free((void *)config.filename);
    free((void *)config.car_name);
    free(config.rows);

    if (error != ESP_OK) return &state_undefined_error;

    return NULL;
}

state_t* logging_active_handler(state_event_id_t event) {
    switch (event)
    {
    case STATE_EVENT_WIFI_DATALOGGER_STOP:
        stop_datalogger_service();
        return &state_logger_idle;
    case STATE_EVENT_TERM_15_OFF:
        stop_datalogger_service();
        return &state_wait_term_15;
    case STATE_EVENT_MEMORY_FULL:
        stop_datalogger_service();
        return &state_memory_full;
    case STATE_EVENT_CAN_ERROR:
        stop_datalogger_service();
        return &state_can_error;
    case STATE_EVENT_LOG_ERROR:
        stop_datalogger_service();
        return &state_log_error;
    default:
        break;
    }
    return &state_logging_active;
}

state_t state_logging_active = {
    .state_id = STATE_LOGGING_ACTIVE,
    .name = "logging_active",
    .handler = logging_active_handler,
    .on_enter = logging_active_enter
};

state_t* logger_idle_handler(state_event_id_t event) {
    switch (event)
    {
    case STATE_EVENT_WIFI_DISCONNECTED:
    case STATE_EVENT_WIFI_DATALOGGER_START:
        return &state_import_config;
    default:
        break;
    }
    return &state_logger_idle;
}

state_t state_logger_idle = {
    .state_id = STATE_LOGGER_IDLE,
    .name = "logger_idle",
    .handler = logger_idle_handler
};

state_t* deepsleep_enter(void) {
    go_to_deep_sleep();
    return NULL;
}

state_t* deepsleep_handler(state_event_id_t event) {
    return &state_deepsleep;
}

state_t state_deepsleep = {
    .state_id = STATE_DEEPSLEEP,
    .name = "deepsleep",
    .handler = deepsleep_handler,
    .on_enter = deepsleep_enter
};