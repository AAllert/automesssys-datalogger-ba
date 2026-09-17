#pragma once

// system includes
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

/**
 * @brief Starts the process of an over the air update.
 *
 * Erases the inactive ota partition so it is ready to receive the new firmware.
 *
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_STATE if an ota update is already in progress
 * @return - ESP_ERR_OTA_ROLLBACK_INVALID_STATE if the running firmware has not been confirmed yet via ota_confirm()
 * @return - ESP_FAIL if an unexpected error occurs
 */
esp_err_t ota_begin(void);

/**
 * @brief Writes the passed bytes of a firmware in the other ota partition.
 *
 * @param[in] buffer a buffer for raw bytes of the new firmware.bin
 * @param[in] size the size of the buffer
 *
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_STATE if ota did not start yet
 *
 * @note It is not possible to load the entire firmware.bin into RAM, because the firmware is to large for this.
 * @note This function may be called repeatedly with successive chunks of the firmware.bin.
 */
esp_err_t ota_write_bytes(const uint8_t *buffer, size_t size);

/**
 * @brief Finishes the ota update. If this function is successful, after esp_restart() the new firmware will be executed.
 *
 * This includes image validation, signature checking, and setting the newly written partition as boot partition.
 *
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_STATE if ota did not start yet
 * @return - ESP_ERR_OTA_VALIDATE_FAILED if the firmware image is invalid.
 *
 * @note This requires an intact and fully written second ota partition.
 * @note On failure, the started ota update is discarded, same as calling ota_abort().
 */
esp_err_t ota_finish(void);

/**
 * @brief Aborts an ota update started with ota_begin(), discarding all data written so far.
 *
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_STATE if ota did not start yet
 */
esp_err_t ota_abort(void);

/**
 * @brief Indicates to the rollback API that the current firmware is functioning properly.
 *
 * This activates the possibility for a new ota update.
 *
 * @return - ESP_OK on success
 */
esp_err_t ota_confirm(void);

/**
 * @brief Executes a rollback to the firmware in the other ota partition and reboots.
 *
 * @return - Does not return on success, the device reboots into the previous firmware.
 * @return - ESP_FAIL if no valid firmware is available to roll back to.
 */
esp_err_t ota_rollback(void);
