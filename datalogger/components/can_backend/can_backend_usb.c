// implements
#include "can_backend.h"

// system includes
#include <stdbool.h>
#include <inttypes.h>
#include "esp_log.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// project inlcudes
#include "usb_serial.h"
#include "can_ascii.h"

#define TAG "USB-CAN"

static bool s_initialized = false;

esp_err_t can_backend_usb_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }
    esp_err_t err = usb_serial_init();
    if (err == ESP_OK) {
        s_initialized = true;
    }
    return err;
}

esp_err_t can_backend_usb_send(const can_frame_t *frame)
{
    char buffer[128];

    esp_err_t err = can_frame_to_ascii(frame, buffer, sizeof(buffer));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "can_frame_to_ascii FAILED: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "USB TX: '%s'", buffer);
    esp_err_t write_err = usb_serial_write_line(buffer);
    if (write_err != ESP_OK) {
        ESP_LOGE(TAG, "usb_write_line FAILED: %s", esp_err_to_name(write_err));
    }
    return write_err;
}

esp_err_t can_backend_usb_receive(can_frame_t *frame, uint32_t timeout_ms)
{
    char line[USB_BUFFER_SIZE];

    TickType_t start    = xTaskGetTickCount();
    TickType_t deadline = start + pdMS_TO_TICKS(timeout_ms);

    while (xTaskGetTickCount() < deadline) {
        uint32_t remaining_ms = pdTICKS_TO_MS(deadline - xTaskGetTickCount());
        if (remaining_ms == 0) break;

        esp_err_t err = usb_serial_read_line(line, sizeof(line), remaining_ms);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "usb_read_line FAILED: %s (timeout=%" PRIu32 "ms) - vECU did not respond in time", esp_err_to_name(err), timeout_ms);
            return err;
        }
        ESP_LOGI(TAG, "USB RX: '%s'", line);

        esp_err_t parse_err = ascii_to_can_frame(line, frame);
        if (parse_err == ESP_OK) {
            return ESP_OK;
        }
        ESP_LOGI(TAG, "Skipping non-CAN line: '%s'", line);
    }

    ESP_LOGE(TAG, "usb_read_line FAILED: %s (timeout=%" PRIu32 "ms) - vECU did not respond in time", esp_err_to_name(ESP_ERR_TIMEOUT), timeout_ms);
    return ESP_ERR_TIMEOUT;
}

esp_err_t can_backend_usb_flush(void)
{
    usb_serial_flush_input();
    return ESP_OK;
}
