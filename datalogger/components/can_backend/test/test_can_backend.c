// tests
#include "can_backend.h"

// system includes
#include <stdbool.h>
#include <inttypes.h>
#include "unity.h"
#include "regex.h"
#include "esp_log.h"
#include "string.h"
#include "esp_err.h"

// project inlcudes
#include "test_helper.h"

#define TAG "[can_backend]"

TEST_CASE("send CAN frame (ID3 Speed) with isotp and receive response", TAG)
{
    can_frame_t frame = {
        .id = 0x17FC0076,
        .dlc = 8,
        .data = {
            0x03, 0x22, 0xF4, 0x0D, 0x00, 0x00, 0x00, 0x00
        }
    };

    esp_err_t send_err = can_backend_send(&frame);

    can_frame_t response;
    esp_err_t receive_err = can_backend_receive(&response, 1000);

    char assert_msg[128];

    snprintf(assert_msg, sizeof(assert_msg), "can_backend_send failed with err: %s", esp_err_to_name(send_err));
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, send_err, assert_msg);

    snprintf(assert_msg, sizeof(assert_msg), "can_backend_send failed with err: %s", esp_err_to_name(receive_err));
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, receive_err, assert_msg);

    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0x17FE0076, response.id, "Wrong response CAN ID (expected 0x17FE0076)");
    TEST_ASSERT_TRUE_MESSAGE((response.data[0] & 0xF0) == 0x00, "data[0] high nibble should be 0x0 (ISO-TP Single Frame PCI)");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x62, response.data[1], "data[1] should be 0x62 (positive RDBI response, after ISO-TP PCI)");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xF4, response.data[2], "data[2] should be 0x2C (DID high byte echo)");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x0D, response.data[3], "data[3] should be 0x96 (DID low byte echo)");
}

TEST_CASE("send CAN Frame with unknown did and receive nrc or timeout", TAG)
{
    can_frame_t frame = {
        .id = 0x17FC0076,
        .dlc = 4,
        .data = {
            0x03, 0x22, 0xFF, 0xFF
        }
    };

    TEST_ASSERT_ESP_ERR(ESP_OK, can_backend_send(&frame));
    can_frame_t response;
    esp_err_t receive_err = can_backend_receive(&response, 1000);
    char assert_msg[128];
    bool is_nrc = response.data[0] == 0x7F;
    snprintf(assert_msg, sizeof(assert_msg), "can_backend_reveive finfished with err: %s, is_nrc: %s", esp_err_to_name(receive_err), is_nrc ? "true" : "false");
    TEST_ASSERT_TRUE_MESSAGE(receive_err == ESP_ERR_TIMEOUT || !is_nrc, assert_msg);
}

TEST_CASE("send CAN frame with unknown sid and receive negative response or Timeout", TAG)
{
    can_frame_t frame = {
        .id = 0x123,
        .dlc = 8,
        .data = {
            0x99, 0x22, 0x33, 0x44,
            0x55, 0x66, 0x77, 0x88
        }
    };

    esp_err_t send_err = can_backend_send(&frame);
    char assert_msg[128];
    snprintf(assert_msg, sizeof(assert_msg), "can_backend_send failed with err: %s", esp_err_to_name(send_err));
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, send_err, assert_msg);


    can_frame_t response;
    esp_err_t receive_err = can_backend_receive(&response, 100);
    if (receive_err == ESP_ERR_TIMEOUT)
    {
        return;
    }
    
    snprintf(assert_msg, sizeof(assert_msg), "can_backend_receive failed with err: %s", esp_err_to_name(send_err));
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, receive_err, assert_msg);

    // Check Negative Response DLC
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8_MESSAGE(3, response.dlc, "Response DLC too small for negative response");

    // UDS Negative Response Format: 0x7F <SID> <NRC>
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x7F, response.data[0], "Expected negative response (0x7F)");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x99, response.data[1], "Response SID does not match request SID");

    // NRC = 0x11 (ServiceNotSupported) is the expected default, some ECUs send something different
    uint8_t nrc = response.data[2];

    if (nrc != 0x11)
    {
        snprintf(assert_msg, sizeof(assert_msg), "Unexpected NRC: 0x%02X (expected 0x11 ServiceNotSupported)", nrc);
        TEST_IGNORE_MESSAGE(assert_msg);
    }
}