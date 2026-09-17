// implements
#include "nvs_storage.h"

//system includes
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_check.h"

#define NVS_UDS_NAMESPACE "uds"
#define NVS_SETTINGS_NAMESPACE "settings"

#define NVS_ACTIVE_CONFIG "active_config"

// NVS keys are limited to 15 characters.
#define NVS_CAN_TIMEOUT "can_timeout"
#define NVS_UDS_TIMEOUT "uds_timeout"
#define NVS_DEEPSLEEP_TIMEOUT "sleep_timeout"
#define NVS_TERM15_INTERVAL "term15_interval"

// Defaults used until a value has been stored via the web interface
#define DEFAULT_CAN_TIMEOUT_MS 100
#define DEFAULT_UDS_TIMEOUT_MS 1000
#define DEFAULT_DEEPSLEEP_TIMEOUT_S 60
#define DEFAULT_TERM15_INTERVAL_MS 1000

#define TAG "nvs"

nvs_handle_t handle;

esp_err_t nvs_init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs flash erase failed");
        ret = nvs_flash_init();
    }
    return ret;
}

esp_err_t erase_key(const char *namespace, const char *key) {
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open(namespace, NVS_READWRITE, &handle), TAG, "nvs open failed");
    esp_err_t result = nvs_erase_key(handle, key);
    if (result != ESP_OK && result != ESP_ERR_NVS_NOT_FOUND) return result;
    ESP_RETURN_ON_ERROR(nvs_commit(handle), TAG, "nvs commit failed");
    return ESP_OK;
}

esp_err_t nvs_set_active_config(const char *filename) {
    if (filename == NULL) return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_UDS_NAMESPACE, NVS_READWRITE, &handle), TAG, "nvs open failed");

    ESP_RETURN_ON_ERROR(nvs_set_str(handle, NVS_ACTIVE_CONFIG, filename), TAG, "nvs set string failed");
    ESP_RETURN_ON_ERROR(nvs_commit(handle), TAG, "nvs commit failed");

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t nvs_get_active_config(char **filename) {
    if (filename == NULL) return ESP_ERR_INVALID_ARG;
    *filename = NULL;

    ESP_RETURN_ON_ERROR(nvs_open(NVS_UDS_NAMESPACE, NVS_READWRITE, &handle), TAG, "NVS open failed");

    size_t length;
    ESP_RETURN_ON_ERROR(nvs_get_str(handle, NVS_ACTIVE_CONFIG, NULL, &length), TAG, "nvs get string failed");
    *filename = malloc(length);
    ESP_RETURN_ON_ERROR(nvs_get_str(handle, NVS_ACTIVE_CONFIG, *filename, &length), TAG, "nvs get string2 failed");
    nvs_close(handle);
    return ESP_OK;
}

static esp_err_t nvs_get_u32_setting(const char *key, uint32_t default_value, uint32_t *out_value) {
    if (out_value == NULL) return ESP_ERR_INVALID_ARG;

    nvs_handle_t settings_handle;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_SETTINGS_NAMESPACE, NVS_READWRITE, &settings_handle), TAG, "nvs open failed");

    esp_err_t result = nvs_get_u32(settings_handle, key, out_value);
    nvs_close(settings_handle);

    if (result == ESP_ERR_NVS_NOT_FOUND) {
        *out_value = default_value;
        return ESP_OK;
    }
    return result;
}

static esp_err_t nvs_set_u32_setting(const char *key, uint32_t value) {
    nvs_handle_t settings_handle;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_SETTINGS_NAMESPACE, NVS_READWRITE, &settings_handle), TAG, "nvs open failed");

    esp_err_t result = nvs_set_u32(settings_handle, key, value);
    if (result == ESP_OK) {
        result = nvs_commit(settings_handle);
    }
    nvs_close(settings_handle);
    return result;
}

esp_err_t nvs_set_can_timeout(uint32_t timeout_ms) {
    return nvs_set_u32_setting(NVS_CAN_TIMEOUT, timeout_ms);
}

esp_err_t nvs_get_can_timeout(uint32_t *timeout_ms) {
    return nvs_get_u32_setting(NVS_CAN_TIMEOUT, DEFAULT_CAN_TIMEOUT_MS, timeout_ms);
}

esp_err_t nvs_set_uds_timeout(uint32_t timeout_ms) {
    return nvs_set_u32_setting(NVS_UDS_TIMEOUT, timeout_ms);
}

esp_err_t nvs_get_uds_timeout(uint32_t *timeout_ms) {
    return nvs_get_u32_setting(NVS_UDS_TIMEOUT, DEFAULT_UDS_TIMEOUT_MS, timeout_ms);
}

esp_err_t nvs_set_deepsleep_timeout(uint32_t timeout_s) {
    return nvs_set_u32_setting(NVS_DEEPSLEEP_TIMEOUT, timeout_s);
}

esp_err_t nvs_get_deepsleep_timeout(uint32_t *timeout_s) {
    return nvs_get_u32_setting(NVS_DEEPSLEEP_TIMEOUT, DEFAULT_DEEPSLEEP_TIMEOUT_S, timeout_s);
}

esp_err_t nvs_set_term15_request_interval(uint32_t interval_ms) {
    return nvs_set_u32_setting(NVS_TERM15_INTERVAL, interval_ms);
}

esp_err_t nvs_get_term15_request_interval(uint32_t *interval_ms) {
    return nvs_get_u32_setting(NVS_TERM15_INTERVAL, DEFAULT_TERM15_INTERVAL_MS, interval_ms);
}
