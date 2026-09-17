#include "datalogger_service.h"

// system includes
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// project includes
#include "app_config.h"
#include "event_loop.h"
#include "uds_config.h"
#include "uds.h"
#include "log_manager.h"
#include "csv_log_manager.h"
#include "uds_decoder.h"
#include "nvs_storage.h"

#define TAG "DATALOGGER"

#define SAMPLING_RATE 20

static TaskHandle_t logger_task = NULL;
static bool is_running = false;
static uint32_t num_cycles = 0;
static uint32_t num_timeouts = 0;

static uint32_t can_timeout = DEFAULT_CAN_TIMEOUT_MS;
static uint32_t uds_timeout = DEFAULT_UDS_TIMEOUT_MS;

static void datalogger_task_loop(void *arg);

static void increment_num_cycles(void) {
    num_cycles++;
    event_loop_publish_system_event(SYSTEM_EVENT_NEW_CYCLE, &num_cycles, sizeof(num_cycles));
}

static void increment_num_timeouts(void) {
    num_timeouts++;
    event_loop_publish_system_event(SYSTEM_EVENT_TIMEOUT_REACHED, &num_timeouts, sizeof(num_timeouts));
}

static void set_num_cycles(uint32_t cycles) {
    num_cycles = cycles;
    event_loop_publish_system_event(SYSTEM_EVENT_NEW_CYCLE, &num_cycles, sizeof(num_cycles));
}

static void set_num_timeouts(uint32_t timeouts) {
    num_timeouts = timeouts;
    event_loop_publish_system_event(SYSTEM_EVENT_TIMEOUT_REACHED, &num_timeouts, sizeof(num_timeouts));
}

uint32_t get_num_cycles(void) {
    return num_cycles;
}

uint32_t get_num_timeouts(void) {
    return num_timeouts;
}

// help function to centralize the free of the config
static void free_config_copy(UdsConfig *config_copy) {
    if (!config_copy) return;
    free((void *)config_copy->filename);
    free((void *)config_copy->car_name);
    free(config_copy->rows);
    free(config_copy);
}

esp_err_t start_datalogger_service(const UdsConfig *config, uint32_t uds_timeout_ms, uint32_t can_timeout_ms) {
    if (is_running) {
        return ESP_OK;
    }

    // deep copy of the callers config
    UdsConfig *config_copy = malloc(sizeof(UdsConfig));
    if (!config_copy) return ESP_ERR_NO_MEM;
    config_copy->filename = strdup(config->filename);
    config_copy->car_name = strdup(config->car_name);
    config_copy->num_rows = config->num_rows;
    config_copy->rows = malloc(config->num_rows * sizeof(ConfigRow));
    if (!config_copy->filename || !config_copy->car_name || !config_copy->rows) {
        free_config_copy(config_copy);
        return ESP_ERR_NO_MEM;
    }
    memcpy(config_copy->rows, config->rows, config->num_rows * sizeof(ConfigRow));

    is_running = true;
    set_num_cycles(0);
    set_num_timeouts(0);

    uds_timeout = uds_timeout_ms;
    can_timeout = can_timeout_ms;

    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_ERROR(start_new_log_file(NULL, config_copy->car_name), cleanup, TAG, "start_new_raw_logfile failed with error: %s", esp_err_to_name(err_rc_));
    ESP_GOTO_ON_ERROR(start_new_csv_log_file(NULL, *config_copy), cleanup, TAG, "start_new_csv_logfile failed with error: %s", esp_err_to_name(err_rc_));

    xTaskCreate(datalogger_task_loop, "datalogger", 8192, config_copy, 7, &logger_task);
    ESP_LOGI(TAG, "Datalogger Started");
    return ESP_OK;

cleanup:
    is_running = false;
    free_config_copy(config_copy);
    return ret;
}

esp_err_t stop_datalogger_service(void) {
    if (!is_running) {
        return ESP_OK;
    }
    is_running = false;
    set_num_cycles(0);
    set_num_timeouts(0);
    ESP_LOGI(TAG, "Datalogger Stopped");
    return ESP_OK;
}

static void datalogger_task_loop(void *arg) {
    UdsConfig *config = arg;
    while(is_running) {
        UdsResponse *responses = calloc(config->num_rows, sizeof(UdsResponse));
        if (responses) {
            request_full_config(*config, responses);
            for (int i=0;i<config->num_rows;i++) {
                LogEntry entry = {
                    .timestamp_us = responses[i].timestamp_us,
                    .data_length = responses[i].size
                };
                if (strlen(responses[i].name) != 0) {
                    strncpy(entry.name, responses[i].name, MAX_SIGNAL_NAME_LENGTH);
                    memcpy(entry.data, responses[i].data, responses[i].size);
                    if(append_to_log(&entry) != ESP_OK) {
                        event_loop_publish_state_event(STATE_EVENT_LOG_ERROR, NULL, 0);
                    }
                }
            }
            if (append_csv_log_row(responses, *config) != ESP_OK) {
                event_loop_publish_state_event(STATE_EVENT_LOG_ERROR, NULL, 0);
            }
            flush_log();    // force write all log entries at once to increase performance
            flush_csv_log();
            free(responses);
        }
        if (is_running) increment_num_cycles();
    }
    
    close_csv_log_file();
    close_log_file();
    free_config_copy(config);
    logger_task = NULL;
    set_num_cycles(0);
    set_num_timeouts(0);
    vTaskDelete(NULL);
}

esp_err_t request_full_config(UdsConfig config, UdsResponse *responses) {
    if (config.num_rows <= 0 || !config.rows || !responses) return ESP_ERR_INVALID_ARG;

    for (int i=0;i<config.num_rows;i++) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        responses[i].timestamp_us = (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
        snprintf(responses[i].name, sizeof(responses[i].name), "%s", config.rows[i].name);

        if (strcmp(config.rows[i].name, "stTerm15") == 0) {
            if(!uds_check_active_ignition(config.rows[i], uds_timeout, can_timeout, &responses[i])) {
                
                event_loop_publish_state_event(STATE_EVENT_TERM_15_OFF, NULL, 0);
                stop_datalogger_service();
                break;
            }
        } else {
            send_uds(
                config.rows[i].canId,
                config.rows[i].sid,
                config.rows[i].requestParameters,
                2,
                config.rows[i].responseCanId,
                uds_timeout,
                can_timeout,
                &responses[i]
            );
            ESP_LOGI(TAG, "send_uds for value %s with did [%d, %d] finished with err: %s", config.rows[i].name, config.rows[i].requestParameters[0], config.rows[i].requestParameters[1], esp_err_to_name(responses[i].status));
            
            if (responses[i].status == ESP_ERR_TIMEOUT) {
                increment_num_timeouts();
            } else if (responses[i].status != ESP_OK) {
                event_loop_publish_state_event(STATE_EVENT_CAN_ERROR, NULL, 0);
            }
        }
    }
    return ESP_OK;
}

bool uds_check_active_ignition(ConfigRow ignition_row, const uint32_t uds_timeout_ms, const uint32_t can_timeout_ms, UdsResponse *response) {
    if (response == NULL) return false;
    send_uds(ignition_row.canId, ignition_row.sid, ignition_row.requestParameters, 2, ignition_row.responseCanId, uds_timeout_ms, can_timeout_ms, response);
    if (response->status != ESP_OK) {
        ESP_LOGW(TAG, "ignition check failed (%s) - treating as inactive", esp_err_to_name(response->status));
        return false;
    }

    uint16_t expected_did = ((uint16_t)ignition_row.requestParameters[0] << 8) | ignition_row.requestParameters[1];
    const uint8_t *payload;
    size_t payload_size;
    esp_err_t extract_err = uds_extract_rdbi_payload(response->data, response->size, expected_did, &payload, &payload_size);
    if (extract_err != ESP_OK) {
        ESP_LOGW(TAG, "ignition check: unexpected response (%s) - treating as inactive", esp_err_to_name(extract_err));
        return false;
    }

    uint32_t raw = extract_bits(payload, ignition_row.startBit, ignition_row.length, ignition_row.byte_order);
    double value = decode_raw_value(raw, ignition_row.scale, ignition_row.offset);

    ESP_LOGI(TAG, "ignition status raw=%"PRIu32" decoded=%.2f", raw, value);
    return value != 0.0;
}