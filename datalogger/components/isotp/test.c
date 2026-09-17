#include "test.h"
#include <stdint.h>
#include "isotp.h"
#include "isotp_defines.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "lib test";

#define CAN_MESSAGE_BYTE_SIZE 8
#define ISOTP_BUFSIZE 4095 // max size of ISO-TP message

const uint8_t data1[] = {0x10, 0x3E, 0x62, 0x01, 0x01, 0xFF, 0xF7, 0xE7};
const uint8_t data3[] = {0x21, 0xFF, 0x96, 0x3E, 0x11, 0x42, 0x90, 0x83};
const uint8_t data4[] = {0x22, 0x00, 0x12, 0x0E, 0xF3, 0x17, 0x16, 0x16};
const uint8_t data5[] = {0x23, 0x16, 0x17, 0x16, 0x00, 0x00, 0x18, 0xC3};
const uint8_t data6[] = {0x24, 0x02, 0xC2, 0x0B, 0x00, 0x00, 0x92, 0x00};
const uint8_t data7[] = {0x25, 0x01, 0x04, 0x50, 0x00, 0x01, 0x02, 0x0E};
const uint8_t data8[] = {0x26, 0x00, 0x63, 0xF3, 0x00, 0x00, 0x00, 0x60};
const uint8_t data9[] = {0x27, 0x74, 0x00, 0x3D, 0xA5, 0x07, 0x0D, 0x01};
const uint8_t data10[] = {0x28, 0x7F, 0x00, 0x00, 0x00, 0x00, 0xAA, 0xAA};
const uint8_t *data[9] = {data1, data3, data4, data5, data6, data7, data8, data9, data10};

static IsoTpLink g_link;
static uint8_t g_isotpRecvBuf[ISOTP_BUFSIZE];
static uint8_t g_isotpSendBuf[ISOTP_BUFSIZE];

void run_lib_test()
{
    ESP_LOGI(TAG, "Running ISO-TP test");

    // Initialize ISO-TP link
    isotp_init_link(&g_link, 0x7FF,
                    g_isotpSendBuf, sizeof(g_isotpSendBuf),
                    g_isotpRecvBuf, sizeof(g_isotpRecvBuf));

    // Wait for CAN to be ready
    vTaskDelay(pdMS_TO_TICKS(100));

    int counter = 0;
    uint8_t payload[ISOTP_BUFSIZE];
    uint16_t received_size;

    while (counter < 9)
    {
        ESP_LOGI(TAG, "Processing frame %d", counter);

        // Process incoming CAN frame
        isotp_on_can_message(&g_link, (uint8_t *)data[counter], CAN_MESSAGE_BYTE_SIZE);

        // Poll for protocol handling
        isotp_poll(&g_link);

        // Check if message is complete

        if (isotp_received_all_data(&g_link))
        {
            isotp_collect_payload(&g_link, payload, sizeof(payload), &received_size);
            ESP_LOGI(TAG, "Received complete message, size: %d", received_size);
            for (int i = 0; i < received_size; i++)
            {
                ESP_LOGI(TAG, "Payload[%d]: 0x%02X", i, payload[i]);
            }
            break;
        }

        // Add delay between frames
        vTaskDelay(pdMS_TO_TICKS(10));
        counter++;
    }

    if (g_link.receive_status != ISOTP_RECEIVE_STATUS_IDLE)
    {
        ESP_LOGE(TAG, "Failed to receive complete message");
    }
}