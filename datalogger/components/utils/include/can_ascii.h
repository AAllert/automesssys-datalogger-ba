#pragma once

// system includes
#include "esp_err.h"

// project includes
#include "can_backend.h"

/**
 * @brief Converts a can frame to its ascii string representation. 
 * 
 * @param[in] frame The CAN frame which should be converted
 * @param[out] buffer A buffer where the output string can be written to.
 * @param[in] buffer_size The size of the buffer.
 * 
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_ARG if an argument is invalid
 */
esp_err_t can_frame_to_ascii(const can_frame_t *frame, char *buffer, size_t buffer_size);

/**
 * @brief Converts a CAN frame to a human-readable String which is stored to the passed buffer.
 * 
 * @param[in] input The ascii line which should be parsed
 * @param[out] frame The ascii line as a CAN frame
 * 
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_ARG if an argument is not valid
 */
esp_err_t ascii_to_can_frame(const char *input, can_frame_t *frame);