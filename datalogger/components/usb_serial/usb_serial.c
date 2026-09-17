#include "usb_serial.h"
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "esp_err.h"

#define BUF_SIZE 1024
#define TAG "usb_serial"

static uint8_t rx_chunk[BUF_SIZE];
static int     rx_chunk_len = 0;
static int     rx_chunk_pos = 0;
static char    line_buf[BUF_SIZE];
static int     line_len = 0;

static bool s_initialized = false;

esp_err_t usb_serial_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }
    usb_serial_jtag_driver_config_t cfg = {
        .rx_buffer_size = BUF_SIZE,
        .tx_buffer_size = BUF_SIZE,
    };
    esp_err_t err = usb_serial_jtag_driver_install(&cfg);
    ESP_LOGI(TAG, "bereit");
    if (err == ESP_OK) {
        s_initialized = true;
    }
    return err;
}

esp_err_t usb_serial_write_line(const char *line)
{
    usb_serial_jtag_write_bytes(line, strlen(line), 20 / portTICK_PERIOD_MS);
    usb_serial_jtag_write_bytes("\n", 1, 20 / portTICK_PERIOD_MS);
    return ESP_OK;
}

esp_err_t usb_serial_read_line(char *buf, size_t buf_size, int timeout_ms)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);

    do {
        // Process all bytes already read into rx_chunk before fetching new ones to ensure that no bytes are lost.
        while (rx_chunk_pos < rx_chunk_len) {
            uint8_t c = rx_chunk[rx_chunk_pos++];
            if (c == '\n' || c == '\r') {
                if (line_len == 0) continue;
                line_buf[line_len] = '\0';
                size_t copy_len = line_len < (int)buf_size - 1 ? line_len : buf_size - 1;
                memcpy(buf, line_buf, copy_len);
                buf[copy_len] = '\0';
                line_len = 0;
                return ESP_OK;
            } else if (line_len < BUF_SIZE - 1) {
                line_buf[line_len++] = c;
            } else {
                // Line buffer full without a newline — reset and report error
                line_len = 0;
                return ESP_ERR_NO_MEM;
            }
        }

        // rx_chunk exhausted — check overall deadline before fetching more bytes
        rx_chunk_pos = 0;
        rx_chunk_len = 0;

        TickType_t now = xTaskGetTickCount();
        if (now >= deadline) break;
        uint32_t remaining_ms = pdTICKS_TO_MS(deadline - now);
        if (remaining_ms == 0) break;

        int n = usb_serial_jtag_read_bytes(rx_chunk, BUF_SIZE - 1, pdMS_TO_TICKS(remaining_ms));
        if (n <= 0) break;
        rx_chunk_len = n;

    } while (xTaskGetTickCount() < deadline);

    return ESP_ERR_TIMEOUT;
}

void usb_serial_flush_input(void)
{
    // Drop whatever was already read into rx_chunk/line_buf but not yet consumed as a line.
    rx_chunk_pos = 0;
    rx_chunk_len = 0;
    line_len = 0;

    // Drain bytes already sitting in the driver's RX FIFO (non-blocking).
    uint8_t discard[64];
    int n;
    do {
        n = usb_serial_jtag_read_bytes(discard, sizeof(discard), 0);
    } while (n > 0);
}
