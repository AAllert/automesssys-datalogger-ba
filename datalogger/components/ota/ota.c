// implements
#include "ota.h"

// system includes
#include <stdbool.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_ota_ops.h"

#define TAG "ota"

static bool ota_in_progress = false;
static esp_ota_handle_t ota_handle = 0;
static const esp_partition_t *update_partition = NULL;

esp_err_t ota_begin(void) {
    if (ota_in_progress) return ESP_ERR_INVALID_STATE;

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) return ESP_FAIL;

    ESP_RETURN_ON_ERROR(esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle), TAG, "esp_ota_begin failed");
    ota_in_progress = true;
    return ESP_OK;
}

esp_err_t ota_write_bytes(const uint8_t *buffer, size_t size) {
    if (!ota_in_progress) return ESP_ERR_INVALID_STATE;
    return esp_ota_write(ota_handle, buffer, size);
}

esp_err_t ota_finish(void) {
    if (!ota_in_progress) return ESP_ERR_INVALID_STATE;
    ota_in_progress = false;

    ESP_RETURN_ON_ERROR(esp_ota_end(ota_handle), TAG, "esp_ota_end failed, image invalid");
    return esp_ota_set_boot_partition(update_partition);
}

esp_err_t ota_abort(void) {
    if (!ota_in_progress) return ESP_ERR_INVALID_STATE;
    ota_in_progress = false;
    return esp_ota_abort(ota_handle);
}

esp_err_t ota_confirm(void) {
    return esp_ota_mark_app_valid_cancel_rollback();
}

esp_err_t ota_rollback(void) {
    // Reboots into the previous firmware on success and never returns here.
    return esp_ota_mark_app_invalid_rollback_and_reboot();
}
