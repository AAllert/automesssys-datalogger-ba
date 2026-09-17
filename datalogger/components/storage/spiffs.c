#include "storage.h"

//system includes
#include "esp_err.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "esp_check.h"

#define TAG "Spiffs"

esp_err_t spiffs_init(size_t max_files)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path             = "/spiffs",
        .partition_label       = "storage",
        .max_files             = max_files,
        .format_if_mount_failed = true,
    };
    ESP_RETURN_ON_ERROR(esp_vfs_spiffs_register(&conf), TAG, "spiffs register failed");

    size_t total = 0, used = 0;
    esp_spiffs_info("storage", &total, &used);
    ESP_LOGI(TAG, "SPIFFS: %d/%d Bytes", used, total);
    return ESP_OK;
}