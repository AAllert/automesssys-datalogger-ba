// implements
#include "uds_config.h"

// system includes
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"

// project includes
#include "config_parser.h"
#include "storage.h"
#include "nvs_storage.h"

#define TAG "uds_config"

static UdsConfig loaded_config;
static int16_t index_term15_row = -1;

static bool is_active_config_loaded() {
    char *filename_new_config;
    get_active_config_name(&filename_new_config);
    bool result = (loaded_config.rows != NULL && strcmp(loaded_config.filename, filename_new_config) == 0);
    free(filename_new_config);
    return result;
}

esp_err_t parse_active_config(void) {
    if (is_active_config_loaded()) return ESP_OK;
    esp_err_t result = ESP_OK;

    char *filename_new_config;
    result = get_active_config_name(&filename_new_config);
    if (result != ESP_OK) goto cleanup;
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s%s", CONFIG_PATH, filename_new_config);
    result = parse_config_file(filepath, &loaded_config, &index_term15_row);

cleanup:
    free(filename_new_config);
    ESP_LOGI(TAG, "parse config finished with error code: %s", esp_err_to_name(result));
    return result;
}

esp_err_t get_active_config(UdsConfig *config) {
    if (config == NULL) return ESP_ERR_INVALID_ARG;
    esp_err_t result = parse_active_config();
    if (result != ESP_OK) return result;
    if (!is_active_config_loaded()) return ESP_FAIL;

    // return a deep copy to avoid manipulation by other components
    config->num_rows = loaded_config.num_rows;
    config->rows = malloc(loaded_config.num_rows * sizeof(ConfigRow));
    config->filename = loaded_config.filename ? strdup(loaded_config.filename) : NULL;
    config->car_name = loaded_config.car_name ? strdup(loaded_config.car_name) : NULL;
    if (config->rows == NULL || config->filename == NULL || config->car_name == NULL) {
        free(config->rows);
        free((void *)config->filename);
        free((void *)config->car_name);
        return ESP_ERR_NO_MEM;
    }
    memcpy(config->rows, loaded_config.rows, loaded_config.num_rows * sizeof(ConfigRow));

    ESP_LOGI(TAG, "loaded config with %d rows", config->num_rows);
    return ESP_OK;
}

esp_err_t get_active_ignition_row(ConfigRow *row) {
    if (index_term15_row == -1) return ESP_ERR_INVALID_STATE;
    esp_err_t result = ESP_OK;
    if (!is_active_config_loaded()) result = parse_active_config();
    if (result != ESP_OK) return result;
    *row = loaded_config.rows[index_term15_row];
    return result;
}

esp_err_t get_active_config_name(char **filename) {
    if (filename == NULL) return ESP_ERR_INVALID_ARG;
    esp_err_t result = ESP_OK;
    *filename = NULL;
    
    result = nvs_get_active_config(filename);
    if (result == ESP_OK) {
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s%s", CONFIG_PATH, *filename);
        if (file_exists(filepath)) {
            return ESP_OK;
        } 
    }

    FileInfo *files = malloc(sizeof(FileInfo));
    size_t num_files = 0;
    if (list_available_configs(files, 0, 1, &num_files)) {
        if (num_files == 0){
            result = ESP_ERR_NOT_FOUND;
            goto cleanup;
        }
        result = set_active_config(files[0].name);
        if (result != ESP_OK) {
            goto cleanup;
        }
        *filename = strdup(files[0].name);
    }
cleanup:
    free(files);
    return result;
}

esp_err_t set_active_config(const char *filename) {
    if (filename == NULL) return ESP_ERR_INVALID_ARG;

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s%s", CONFIG_PATH, filename);

    if (file_exists(filepath)){
        return nvs_set_active_config(filename);
    } else {
        return ESP_ERR_NOT_FOUND;
    }
}

bool list_available_configs(FileInfo *files, size_t offset, size_t max_files, size_t *num_files) {
    return storage_list_files(CONFIG_PATH, ".csv", files, offset, max_files, num_files);
}

bool is_config_available(char *filename) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s%s", CONFIG_PATH, filename);
    return file_exists(filepath);
}

void did_to_bytes(uint16_t did, uint8_t bytes[2]) {
    bytes[0] = (did >> 8) & 0xFF;
    bytes[1] = did & 0xFF;
}

bool parse_dbc(const char *encoding, uint16_t *startbit, uint8_t *length, uint8_t *byte_order) 
{
    unsigned int sb, len, order;
    if (!encoding || sscanf(encoding, "%u|%u@%u", &sb, &len, &order) != 3) {
        return false;
    }
    *startbit = (uint16_t)sb;
    *length = (uint8_t)len;
    *byte_order = (uint8_t)order;
    return true;
}