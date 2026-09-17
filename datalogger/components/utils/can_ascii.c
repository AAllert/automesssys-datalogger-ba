#include "esp_err.h"
#include "can_ascii.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "esp_log.h"
#include "can_backend.h"

esp_err_t can_frame_to_ascii(const can_frame_t *frame, char *buffer, size_t buffer_size)
{
    if (!frame || !buffer) return ESP_ERR_INVALID_ARG;
    if (frame->dlc > 8) return ESP_ERR_INVALID_SIZE;

    int pos = 0;

    pos += snprintf(buffer + pos, buffer_size - pos, "CAN %lX %u ", frame->id, frame->dlc);

    for (uint8_t i = 0; i < frame->dlc; i++) {
        pos += snprintf(buffer + pos, buffer_size - pos, "%02X ", frame->data[i]);
    }

    if (pos > 0 && buffer[pos - 1] == ' ') {
        pos--;
    }

    buffer[pos] = '\0';

    return ESP_OK;
}

esp_err_t ascii_to_can_frame(const char *input, can_frame_t *frame)
{
    if (input == NULL || frame == NULL) return ESP_ERR_INVALID_ARG;

    if (strncmp(input, "CAN ", 4) != 0) {
        ESP_LOGE("CAN_UART", "Invalid CAN frame format: %s", input);
        return ESP_ERR_INVALID_ARG;
    }

    // initilize entire can frame, avoid undefined behavior
    memset(frame->data, 0, sizeof(frame->data));

    // copy string
    char buffer[128];
    strncpy(buffer, input, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    // split in tokens
    char *saveptr;
    char *token = strtok_r(buffer, " ", &saveptr);

    if (token == NULL || strcmp(token, "CAN") != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // CAN-ID
    token = strtok_r(NULL, " ", &saveptr);
    if (token == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    frame->id = (uint32_t)strtoul(token, NULL, 16);

    // DLC
    token = strtok_r(NULL, " ", &saveptr);
    if (token == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t expected_dlc = (uint8_t)strtoul(token, NULL, 10);
    if (expected_dlc > 8) {
        ESP_LOGE("CAN_UART", "DLC out of range: %u", expected_dlc);
        return ESP_ERR_INVALID_SIZE;
    }

    // databytes
    uint8_t dlc = 0;

    while ((token = strtok_r(NULL, " ", &saveptr)) != NULL) {
        if (dlc >= 8) {
            ESP_LOGE("CAN_UART", "Too many data bytes");
            return ESP_ERR_INVALID_ARG;
        }

        frame->data[dlc++] = (uint8_t)strtoul(token, NULL, 16);
    }

    frame->dlc = expected_dlc;

    if (frame->dlc == 0) {
        ESP_LOGE("CAN_UART", "CAN frame without data bytes");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}