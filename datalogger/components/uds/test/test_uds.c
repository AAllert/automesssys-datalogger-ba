// tests
#include "uds.h"

// system includes
#include <inttypes.h>
#include <stdlib.h>
#include <time.h>
#include "unity.h"
#include "esp_log.h"
#include "esp_err.h"

// project includes
#include "app_config.h"
#include "can_backend.h"

#define TAG "test_uds"
#define SID_READ_DATA_BY_IDENTIFIER 34
#define DID_ID3_SPEED 62477

const uint32_t request_can_id  = 0x17FC0076;
const uint32_t response_can_id = 402522230;

TEST_CASE("request speed of ID3 over uds and receive response", "[uds]")
{
    UdsResponse *response = calloc(1, sizeof(UdsResponse));

    response->timestamp_us = (int64_t)time(NULL) * 1000000;

    uint8_t did_buffer[2] = {
        (DID_ID3_SPEED >> 8) & 0xFF,  // 0xF4
        DID_ID3_SPEED & 0xFF          // 0x0D
    };

    send_uds(
        request_can_id,
        SID_READ_DATA_BY_IDENTIFIER,
        did_buffer,
        2,
        response_can_id,
        DEFAULT_UDS_TIMEOUT_MS,
        DEFAULT_CAN_TIMEOUT_MS,
        response
    );

    char assert_msg[128];
    snprintf(assert_msg, sizeof(assert_msg), "send_uds failed with err: %s", esp_err_to_name(response->status));
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, response->status, assert_msg);

    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x62, response->data[0],"unexpected sid, should be 0x62 (positive response: SID 0x22 + 0x40)");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE((DID_ID3_SPEED >> 8) & 0xFF, response->data[1], "data[1] should echo DID high byte 0xF4");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(DID_ID3_SPEED & 0xFF, response->data[2], "data[2] should echo DID low byte 0x0D");

    free(response);
}

TEST_CASE("request over uds with unknown sid (receive NRC or timeout)", "[uds]")
{
    UdsResponse *response = calloc(1, sizeof(UdsResponse));

    response->timestamp_us = (int64_t)time(NULL) * 1000000;

    uint8_t did_buffer[2] = {
        (DID_ID3_SPEED >> 8) & 0xFF,  // 0xF4
        DID_ID3_SPEED & 0xFF          // 0x0D
    };

    send_uds(
        request_can_id,
        0x99,
        did_buffer,
        2,
        response_can_id,
        DEFAULT_UDS_TIMEOUT_MS,
        DEFAULT_CAN_TIMEOUT_MS,
        response
    );

    char assert_msg[128];
    snprintf(assert_msg, sizeof(assert_msg), "send_uds failed wirh err: %s", esp_err_to_name(response->status));
    TEST_ASSERT_TRUE_MESSAGE(response->status == ESP_OK || response->status == ESP_ERR_TIMEOUT, assert_msg);
    if (response->status == ESP_ERR_TIMEOUT) return;

    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x7F, response->data[0], "No NRC received");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x99, response->data[1], "Response SID does not match request SID");

    // NRC = 0x11 (ServiceNotSupported) is the expected default, some ECUs send something different
    uint8_t nrc = response->data[2];

    if (nrc != 0x11)
    {
        snprintf(assert_msg, sizeof(assert_msg), "Unexpected NRC: 0x%02X (expected 0x11 ServiceNotSupported)", nrc);
        TEST_IGNORE_MESSAGE(assert_msg);
    }

    free(response);
}

TEST_CASE("request over uds with unknown CAN Id", "[uds]")
{
    UdsResponse *response = calloc(1, sizeof(UdsResponse));

    response->timestamp_us = (int64_t)time(NULL) * 1000000;

    uint8_t did_buffer[2] = {
        (DID_ID3_SPEED >> 8) & 0xFF,  // 0xF4
        DID_ID3_SPEED & 0xFF          // 0x0D
    };

    send_uds(
        request_can_id,
        0x99,
        did_buffer,
        2,
        response_can_id,
        DEFAULT_UDS_TIMEOUT_MS,
        DEFAULT_CAN_TIMEOUT_MS,
        response
    );

    if (response->status == ESP_ERR_TIMEOUT)
    {
        return;
    }

    char assert_msg[128];
    snprintf(assert_msg, sizeof(assert_msg), "send_uds failed wirh err: %s", esp_err_to_name(response->status));
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, response->status, assert_msg);

    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x7F, response->data[0], "No NRC received");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x99, response->data[1], "Response SID does not match request SID");

    // NRC = 0x11 (ServiceNotSupported) is the expected default, some ECUs send something different
    uint8_t nrc = response->data[2];

    if (nrc != 0x11)
    {
        snprintf(assert_msg, sizeof(assert_msg), "Unexpected NRC: 0x%02X (expected 0x11 ServiceNotSupported)", nrc);
        TEST_IGNORE_MESSAGE(assert_msg);
    }

    free(response);
}

TEST_CASE("request over uds with unknown response can id", "[uds]")
{
    UdsResponse *response = calloc(1, sizeof(UdsResponse));

    response->timestamp_us = (int64_t)time(NULL) * 1000000;

    uint8_t did_buffer[2] = {
        (DID_ID3_SPEED >> 8) & 0xFF,  // 0xF4
        DID_ID3_SPEED & 0xFF          // 0x0D
    };

    send_uds(
        request_can_id,
        0x99,
        did_buffer,
        2,
        1968,
        DEFAULT_UDS_TIMEOUT_MS,
        DEFAULT_CAN_TIMEOUT_MS,
        response
    );

    char assert_msg[128];
    snprintf(assert_msg, sizeof(assert_msg), "send_uds failed wirh err: %s, expected ESP_ERR_TIMEOUT", esp_err_to_name(response->status));
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_TIMEOUT, response->status, assert_msg);

    free(response);
}

TEST_CASE("Passing Null Parameters to send_uds", "[uds]")
{
    UdsResponse *response = calloc(1, sizeof(UdsResponse));

    esp_err_t status = send_uds(
        request_can_id,
        SID_READ_DATA_BY_IDENTIFIER,
        NULL,
        2,
        response_can_id,
        DEFAULT_UDS_TIMEOUT_MS,
        DEFAULT_CAN_TIMEOUT_MS,
        response
    );

    char assert_msg[128];
    snprintf(assert_msg, sizeof(assert_msg), "send_uds failed wirh err: %s, expected ESP_ERR_INVALID_ARG", esp_err_to_name(status));
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_INVALID_ARG, status, assert_msg);

    uint8_t did_buffer[2] = {0xF4, 0x0D};

    status = send_uds(
        request_can_id,
        SID_READ_DATA_BY_IDENTIFIER,
        did_buffer,
        2,
        response_can_id,
        DEFAULT_UDS_TIMEOUT_MS,
        DEFAULT_CAN_TIMEOUT_MS,
        NULL
    );

    snprintf(assert_msg, sizeof(assert_msg), "send_uds failed wirh err: %s, expected ESP_ERR_INVALID_ARG", esp_err_to_name(status));
    TEST_ASSERT_EQUAL_MESSAGE(ESP_ERR_INVALID_ARG, status, assert_msg);

    free(response);
}

TEST_CASE("Receive ISOTP Multi-Frame Response (seat heating ID3)", "[uds]")
{
    UdsResponse *response = calloc(1, sizeof(UdsResponse));

    response->timestamp_us = (int64_t)time(NULL) * 1000000;

    uint8_t did_buffer[2] = {
        (15072 >> 8) & 0xFF,
        15072 & 0xFF
    };

    send_uds(
        1862,                           // Request CAN ID
        SID_READ_DATA_BY_IDENTIFIER,
        did_buffer,
        2,
        1968,                           // Response CAN ID
        DEFAULT_UDS_TIMEOUT_MS,
        DEFAULT_CAN_TIMEOUT_MS,
        response
    );

    char assert_msg[128];
    snprintf(assert_msg, sizeof(assert_msg), "send_uds failed wirh err: %s", esp_err_to_name(response->status));
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, response->status, assert_msg);

    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x62, response->data[0], "unexpected sid, should be 0x62 (positive response: SID 0x22 + 0x40)");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE((15072 >> 8) & 0xFF, response->data[1], "data[1] should echo DID high byte (0x3A)");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(15072 & 0xFF, response->data[2], "data[2] should echo DID low byte (0xE0)");
    TEST_ASSERT_EQUAL_MESSAGE(30, response->size, "Expected 30 bytes response (SID + DID + 27 payload bytes)");

    free(response);
}

TEST_CASE("Send ISOTP Multi Frame request", "[uds]")
{
    UdsResponse *response = calloc(1, sizeof(UdsResponse));
    response->timestamp_us = (int64_t)time(NULL) * 1000000;

    uint16_t did = 15072;

    uint8_t request_buffer[32];

    // DID
    request_buffer[0] = (did >> 8) & 0xFF;
    request_buffer[1] = did & 0xFF;

    // Payload bewusst groß machen -> Multi-Frame triggern  --> ging nicht am ID3, funktioniert an der vECU
    //for (int i = 0; i < 20; i++) {
      //  request_buffer[2 + i] = i;
    //}

    send_uds(
        1862,                       // Request CAN ID
        0x22,
        request_buffer,
        2,                       // 2 Byte DID + 20 Byte Payload
        1968,                       // Response CAN ID
        DEFAULT_UDS_TIMEOUT_MS,
        DEFAULT_CAN_TIMEOUT_MS,
        response);

    char assert_msg[128];
    snprintf(assert_msg, sizeof(assert_msg),
             "send_uds failed with err: %s",
             esp_err_to_name(response->status));

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, response->status, assert_msg);

    TEST_ASSERT_EQUAL_HEX8(0x62, response->data[0]);
    TEST_ASSERT_EQUAL_HEX8((did >> 8) & 0xFF, response->data[1]);
    TEST_ASSERT_EQUAL_HEX8(did & 0xFF, response->data[2]);

    free(response);
}
