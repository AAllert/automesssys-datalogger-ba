// implements
#include "rest_endpoints.h"

// system includes
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cJSON.h>

// project inlcudes
#include "app_config.h"
#include "event_loop.h"
#include "storage.h"
#include "uds_config.h"
#include "log_manager.h"
#include "csv_log_manager.h"
#include "state_machine.h"
#include "nvs_storage.h"
#include "ota.h"

#define TAG "rest_api"

/**
 * @brief Reads the full request body into a buffer
 * 
 * If an error occurs, an error response will be sent to the client.
 * 
 * @param[in] req The request to read the body of
 * @param[out] buf the buffer containing the request body
 * @param[out] buf_len the length of the buffer
 * 
 * @return - ESP_OK if the body was parsed successfully
 * @return - ESP_FAIL if an error occurred and response code 500 was sent
 * 
 * @note The buffer contains room for the null terminator
 */
static esp_err_t read_request_body(httpd_req_t *req, char *buf, size_t buf_len) {
    int total_len = req->content_len;
    if (total_len <= 0 || (size_t)total_len >= buf_len) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request body");
        return ESP_FAIL;
    }

    int received = 0;
    while (received < total_len) {
        int ret = httpd_req_recv(req, buf + received, total_len - received);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
            return ESP_FAIL;
        }
        received += ret;
    }
    buf[total_len] = '\0';
    return ESP_OK;
}

// configuration

static esp_err_t set_time_handler(httpd_req_t *req) {
    state_t *current_state = state_machine_get_state();
    if (current_state->state_id == STATE_LOGGING_ACTIVE) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char buf[257];
    if (read_request_body(req, buf, sizeof(buf)) != ESP_OK) {
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    cJSON *timestamp = cJSON_GetObjectItem(root, "timestamp");
    if (!cJSON_IsNumber(timestamp)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid 'timestamp'");
        return ESP_FAIL;
    }
    time_t unix_time = (time_t)timestamp->valuedouble;
    cJSON_Delete(root);

    struct timeval tv = {
        .tv_sec = unix_time,
        .tv_usec = 0
    };
    settimeofday(&tv, NULL);
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

static esp_err_t get_time_handler(httpd_req_t *req) {
    time_t now;
    time(&now);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "timestamp", (double)now);
    char *json = cJSON_PrintUnformatted(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t get_active_config_handler(httpd_req_t *req) {
    UdsConfig config = {0};
    if (get_active_config(&config) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "active_config", config.filename);
    cJSON_AddNumberToObject(root, "values", config.num_rows);
    char *json = cJSON_PrintUnformatted(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t result = httpd_resp_sendstr(req, json);

    free(json);
    cJSON_Delete(root);
    free((void *)config.filename);
    free((void *)config.car_name);
    free(config.rows);
    return result;
}

static esp_err_t set_active_config_handler(httpd_req_t *req) {
    state_t *current_state = state_machine_get_state();
    if (
        current_state->state_id == STATE_WAIT_TERM15 ||
        current_state->state_id == STATE_LOGGING_ACTIVE ||
        current_state->state_id == STATE_IMPORT_CONFIG
    ) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char buf[257];
    if (read_request_body(req, buf, sizeof(buf)) != ESP_OK) {
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *config_file = cJSON_GetObjectItem(root, "config_file");
    if (!cJSON_IsString(config_file) || config_file->valuestring[0] == '\0') {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid 'config_file'");
        return ESP_FAIL;
    }

    char filename[MAX_FILENAME_LENGTH];
    strncpy(filename, config_file->valuestring, sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';
    cJSON_Delete(root);

    if (!is_config_available(filename)) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    if (set_active_config(filename) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    return httpd_resp_send(req, NULL, 0);
}

// Sends the value returned by getter as {"json_key": value}.
static esp_err_t get_uint32_setting_handler(httpd_req_t *req, const char *json_key, esp_err_t (*getter)(uint32_t *)) {
    uint32_t value;
    if (getter(&value) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, json_key, value);
    char *json = cJSON_PrintUnformatted(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t result = httpd_resp_sendstr(req, json);

    free(json);
    cJSON_Delete(root);
    return result;
}

/**
 * @brief Helper function for nvs- setting setter endpoints. 
 * 
 * Parses the json from the request body, validates it against a range and persists it.
 * 
 * @param[in] request The reuest to process
 * @param[in] json_key the key to parse the body for
 * @param[in] min_value minimum of the valid range for vilidation
 * @param[in] max_value maximum of the valid range for validation
 * @param[in] setter Callback for a setter function to persist the value
 * @param[in] apply Callback for a function to apply the value immediatly, without waitung for a reboot.
 */
static esp_err_t set_uint32_setting_handler(httpd_req_t *req, const char *json_key, uint32_t min_value, uint32_t max_value,
                                             esp_err_t (*setter)(uint32_t), void (*apply)(uint32_t)) {
    char buf[257];
    if (read_request_body(req, buf, sizeof(buf)) != ESP_OK) {
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *value = cJSON_GetObjectItem(root, json_key);
    if (!cJSON_IsNumber(value) || value->valuedouble < min_value || value->valuedouble > max_value) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid value");
        return ESP_FAIL;
    }
    uint32_t setting_value = (uint32_t)value->valuedouble;
    cJSON_Delete(root);

    if (setter(setting_value) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    if (apply != NULL) {
        apply(setting_value);
    }
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t get_can_timeout_handler(httpd_req_t *req) {
    return get_uint32_setting_handler(req, "can_timeout", nvs_get_can_timeout);
}

static esp_err_t set_can_timeout_handler(httpd_req_t *req) {
    return set_uint32_setting_handler(req, "can_timeout", 10, UINT32_MAX, nvs_set_can_timeout, NULL);
}

static esp_err_t get_uds_timeout_handler(httpd_req_t *req) {
    return get_uint32_setting_handler(req, "uds_timeout", nvs_get_uds_timeout);
}

static esp_err_t set_uds_timeout_handler(httpd_req_t *req) {
    return set_uint32_setting_handler(req, "uds_timeout", 10, UINT32_MAX, nvs_set_uds_timeout, NULL);
}

static esp_err_t get_deepsleep_timeout_handler(httpd_req_t *req) {
    return get_uint32_setting_handler(req, "deepsleep_timeout", nvs_get_deepsleep_timeout);
}

static esp_err_t set_deepsleep_timeout_handler(httpd_req_t *req) {
    return set_uint32_setting_handler(req, "deepsleep_timeout", 10, 600, nvs_set_deepsleep_timeout, set_sleep_timeout_s);
}

static esp_err_t get_term15_request_interval_handler(httpd_req_t *req) {
    return get_uint32_setting_handler(req, "request_interval", nvs_get_term15_request_interval);
}

static esp_err_t set_term15_request_interval_handler(httpd_req_t *req) {
    return set_uint32_setting_handler(req, "request_interval", 100, 10000, nvs_set_term15_request_interval, set_term15_request_interval_ms);
}

// datalogger

static esp_err_t start_logging_handler(httpd_req_t *req) {
    event_loop_publish_state_event(STATE_EVENT_WIFI_DATALOGGER_START, NULL, 0);
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t stop_logging_handler(httpd_req_t *req) {
    event_loop_publish_state_event(STATE_EVENT_WIFI_DATALOGGER_STOP, NULL, 0);
    return httpd_resp_send(req, NULL, 0);
}

// file

/**
 * @brief Returns a list of files to the requester as a json array for the given filepath
 * 
 * @note Shared implementation for the /configs, /logs and /csv-logs listing endpoints.
 *
 * @param[in] req The request to process
 * @param[in] lister Callback for the specific list_files function
 */
static esp_err_t list_files_handler(httpd_req_t *req, bool (*lister)(FileInfo *, size_t, size_t, size_t *)) {
    size_t offset = 0;
    size_t limit = 20;

    char query[64];
    httpd_req_get_url_query_str(req, query, sizeof(query));

    char value[8];
    if (httpd_query_key_value(query, "offset", value, sizeof(value)) == ESP_OK) {
        offset = strtol(value, NULL, 10);
    }

    if (httpd_query_key_value(query, "limit", value, sizeof(value)) == ESP_OK) {
        limit = strtol(value, NULL, 10);
    }
    if (limit == 0) limit = 20;
    if (limit > 100) limit = 100;

    FileInfo *files = calloc(limit, sizeof(FileInfo));
    if (files == NULL) return httpd_resp_send_500(req);

    size_t num_files;
    if (!lister(files, offset, limit, &num_files)) {
        free(files);
        return httpd_resp_send_500(req);
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *entries = cJSON_AddArrayToObject(root, "entries");

    for (size_t i = 0; i < num_files; i++) {
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "filename", files[i].name);
        cJSON_AddNumberToObject(entry, "size", files[i].size);
        cJSON_AddItemToArray(entries, entry);
    }
    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    esp_err_t result = httpd_resp_sendstr(req, json);

    free(json);
    cJSON_Delete(root);
    free(files);
    return result;
}

static esp_err_t list_configs_handler(httpd_req_t *req) {
    // TODO parse and return car name and value count
    return list_files_handler(req, list_available_configs);
}

static esp_err_t list_logs_handler(httpd_req_t *req) {
    // TODO parse and return car name, signal count and cycle count
    return list_files_handler(req, list_collected_logs);
}

static esp_err_t list_csv_logs_handler(httpd_req_t *req) {
    // TODO parse and return car name, signal count and cycle count
    return list_files_handler(req, list_collected_csv_logs);
}

static esp_err_t upload_file_handler(httpd_req_t *req) {
    char path[MAX_STORAGE_PATH_LENGTH];
    if (!storage_resolve_data_path(req->uri, path, sizeof(path))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Upload Request");
        return ESP_FAIL;
    }

    if (file_exists(path)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Upload Request");
        return ESP_FAIL;
    }

    int total_len = req->content_len;
    char *buf = malloc(total_len + 1);
    if (buf == NULL) {
        httpd_resp_set_status(req, "507 Insufficient Storage");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "Insufficient storage");
        return ESP_FAIL;
    }

    int received = 0;
    while (received < total_len) {
        int ret = httpd_req_recv(req, buf + received, total_len - received);
        if (ret <= 0) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
            return ESP_FAIL;
        }
        received += ret;
    }
    buf[total_len] = '\0';

    esp_err_t error = storage_write_file(path, buf);
    free(buf);

    if (error != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }

    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t delete_file_handler(httpd_req_t *req) {
    char path[MAX_STORAGE_PATH_LENGTH];
    if (!storage_resolve_data_path(req->uri, path, sizeof(path))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }

    if (!file_exists(path)) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    if (storage_delete_file(path) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }

    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// ota

#define OTA_CHUNK_SIZE 1024

static esp_err_t ota_update_handler(httpd_req_t *req) {
    state_t *current_state = state_machine_get_state();
    if (
        current_state->state_id == STATE_WAIT_TERM15 ||
        current_state->state_id == STATE_LOGGING_ACTIVE ||
        current_state->state_id == STATE_IMPORT_CONFIG
    ) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    if (req->content_len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request body");
        return ESP_FAIL;
    }

    if (ota_begin() != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char buf[OTA_CHUNK_SIZE];
    int remaining = req->content_len;
    while (remaining > 0) {
        int to_read = remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf);
        int received = httpd_req_recv(req, buf, to_read);
        if (received <= 0) {
            ota_abort();
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
            return ESP_FAIL;
        }
        if (ota_write_bytes((uint8_t *)buf, received) != ESP_OK) {
            ota_abort();
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware write failed");
            return ESP_FAIL;
        }
        remaining -= received;
    }

    if (ota_finish() != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid firmware image");
        return ESP_FAIL;
    }

    httpd_resp_send(req, NULL, 0);

    // Give the response time to flush to the client before rebooting into the new firmware.
    ESP_LOGI(TAG, "OTA update finished, restarting");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK; // unreachable but necessary for the compiler
}

// configuration

httpd_uri_t uri_set_time = {
    .uri = "/time",
    .method = HTTP_PUT,
    .handler = set_time_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_get_time = {
    .uri = "/time",
    .method = HTTP_GET,
    .handler = get_time_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_get_active_config = {
    .uri = "/active-uds-config",
    .method = HTTP_GET,
    .handler = get_active_config_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_set_active_config = {
    .uri = "/active-uds-config",
    .method = HTTP_PUT,
    .handler = set_active_config_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_get_can_timeout = {
    .uri = "/can-timeout",
    .method = HTTP_GET,
    .handler = get_can_timeout_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_set_can_timeout = {
    .uri = "/can-timeout",
    .method = HTTP_PUT,
    .handler = set_can_timeout_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_get_uds_timeout = {
    .uri = "/uds-timeout",
    .method = HTTP_GET,
    .handler = get_uds_timeout_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_set_uds_timeout = {
    .uri = "/uds-timeout",
    .method = HTTP_PUT,
    .handler = set_uds_timeout_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_get_deepsleep_timeout = {
    .uri = "/deepsleep-timeout",
    .method = HTTP_GET,
    .handler = get_deepsleep_timeout_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_set_deepsleep_timeout = {
    .uri = "/deepsleep-timeout",
    .method = HTTP_PUT,
    .handler = set_deepsleep_timeout_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_get_term15_request_interval = {
    .uri = "/term15-request-interval",
    .method = HTTP_GET,
    .handler = get_term15_request_interval_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_set_term15_request_interval = {
    .uri = "/term15-request-interval",
    .method = HTTP_PUT,
    .handler = set_term15_request_interval_handler,
    .user_ctx = NULL
};

// datalogger

httpd_uri_t uri_start_logging = {
    .uri = "/datalogger/start",
    .method = HTTP_POST,
    .handler = start_logging_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_stop_logging = {
    .uri = "/datalogger/stop",
    .method = HTTP_POST,
    .handler = stop_logging_handler,
    .user_ctx = NULL
};

// file

httpd_uri_t uri_list_configs = {
    .uri = "/configs",
    .method = HTTP_GET,
    .handler = list_configs_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_list_logs = {
    .uri = "/logs",
    .method = HTTP_GET,
    .handler = list_logs_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_list_csv_logs = {
    .uri = "/csv-logs",
    .method = HTTP_GET,
    .handler = list_csv_logs_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_upload_file = {
    .uri = "/*",
    .method = HTTP_POST,
    .handler = upload_file_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_delete_file = {
    .uri = "/*",
    .method = HTTP_DELETE,
    .handler = delete_file_handler,
    .user_ctx = NULL
};

// ota

httpd_uri_t uri_ota_update = {
    .uri = "/ota",
    .method = HTTP_POST,
    .handler = ota_update_handler,
    .user_ctx = NULL
};
