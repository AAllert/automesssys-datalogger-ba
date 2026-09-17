// tests
#include "uds.h"
#include "datalogger_service.h"

// system includes
#include <inttypes.h>
#include <stdlib.h>
#include <time.h>
#include "unity.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"

// project inlcudes
#include "uds_config.h"
#include "app_config.h"


TEST_CASE("check ignition id3", "[check_term15_id3]")
{
    ConfigRow ignition_row = {
        .name = "stTerm15",
        .canId = 402391163,
        .sid = 0x22,
        .requestParameters = {0x02, 0xB2},
        .responseCanId = 402522235,
        .numBytes = 1,
        .startBit = 7,
        .length = 1,
        .byte_order = 0,
        .strType = UINT,
        .offset = 0,
        .scale = 1,
        .should_interpolate = false,
    };
    UdsResponse response = {0};

    bool result = uds_check_active_ignition(ignition_row, DEFAULT_UDS_TIMEOUT_MS, DEFAULT_CAN_TIMEOUT_MS, &response);
    ESP_LOGI("uds", "Zündungsstatus ID3: %s", result ? "AN" : "AUS");
    char assert_msg[128];
    snprintf(assert_msg, sizeof(assert_msg), "Ignition ID3: %s", result ? "AN" : "AUS");
    TEST_IGNORE_MESSAGE(assert_msg);
}

TEST_CASE("check ignition mirai 1", "[check_term15_mirai1]")
{
    ConfigRow ignition_row = {
        .name = "stTerm15",
        .canId = 2001,
        .sid = 0x21,
        .requestParameters = {0x00, 0x50},
        .responseCanId = 2009,
        .numBytes = 32,
        .startBit = 95,
        .length = 8,
        .byte_order = 0,
        .strType = UINT,
        .offset = 0,
        .scale = 10,
        .should_interpolate = false,
    };
    UdsResponse response = {0};
    bool result = uds_check_active_ignition(ignition_row, DEFAULT_UDS_TIMEOUT_MS, DEFAULT_CAN_TIMEOUT_MS, &response);
    ESP_LOGI("uds", "Zündungsstatus Mirai1: %s", result ? "AN" : "AUS");
    char assert_msg[128];
    snprintf(assert_msg, sizeof(assert_msg), "Ignition Mirai1: %s", result ? "AN" : "AUS");
    TEST_IGNORE_MESSAGE(assert_msg);
}

TEST_CASE("check ignition mirai 2", "[check_term15_mirai2]")
{
    uint8_t did_buffer[2];
    did_to_bytes(4097, did_buffer);
    ConfigRow ignition_row = {
        .name = "stTerm15",
        .canId = 2003,
        .sid = 0x22,
        .requestParameters = {0x10, 0x01},
        .responseCanId = 2011,
        .numBytes = 2,
        .startBit = 9,
        .length = 1,
        .byte_order = 0,
        .strType = UINT,
        .offset = 0,
        .scale = 1,
        .should_interpolate = false,
    };
    UdsResponse response = {0};

    bool result = uds_check_active_ignition(ignition_row, DEFAULT_UDS_TIMEOUT_MS, DEFAULT_CAN_TIMEOUT_MS, &response);
    ESP_LOGI("uds", "Zündungsstatus Mirai2: %s", result ? "AN" : "AUS");
    char assert_msg[128];
    snprintf(assert_msg, sizeof(assert_msg), "Ignition Mirai2: %s", result ? "AN" : "AUS");
    TEST_IGNORE_MESSAGE(assert_msg);
}

TEST_CASE("check time of id3 tqEmCalc", "[check_tqEmCalc]")
{
    uint8_t did_buffer[2];
    did_to_bytes(776, did_buffer);

    UdsResponse response = {0};

    int64_t start_us = esp_timer_get_time();
    send_uds(402391164, 0x22, did_buffer, 2, 402522236, 5000, 200, &response);
    int64_t elapsed_ms = (esp_timer_get_time() - start_us) / 1000;

    char assert_msg[128];
    snprintf(assert_msg, sizeof(assert_msg), "Antwortzeit tqEmCalc (ID3): %" PRId64 " ms", elapsed_ms);
    ESP_LOGI("uds", "%s", assert_msg);

    TEST_ASSERT_LESS_THAN_MESSAGE(50, elapsed_ms, assert_msg);
}
