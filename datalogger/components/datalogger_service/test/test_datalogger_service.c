// tests
#include "datalogger_service.h"

// system includes
#include <time.h>
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// project inlcudes
#include "test_helper.h"
#include "app_config.h"

#define TAG "[datalogger_service]"

static ConfigRow mockRows[] = {
    {
        .name = "wHvBatDchrg",
        .canId = 402391163,
        .sid = 0x22,
        .requestParameters = (uint8_t[]) {0x1E, 0x32},
        .responseCanId = 402522235,
        .numBytes = 16,
        .startBit = 103,
        .length = 32,
        .byte_order = 0,
        .strType = INT,
        .offset = 0,
        .scale = 550,
        .should_interpolate = true,
    },
    {
        .name = "tqEmMechSet",
        .canId = 402391164,
        .sid = 0x22,
        .requestParameters = (uint8_t[]) {0x58, 0xE1},
        .responseCanId = 402522236,
        .numBytes = 4,
        .startBit = 7,
        .length = 32,
        .byte_order = 0,
        .strType = FLOAT32,
        .offset = 0,
        .scale = 1,
        .should_interpolate = true,
    },
    {
        .name = "stTerm15",
        .canId = 402391163,
        .sid = 0x22,
        .requestParameters = (uint8_t[]){0x02, 0xB2},
        .responseCanId = 402522235,
        .numBytes = 1,
        .startBit = 7,
        .length = 1,
        .byte_order = 0,
        .strType = UINT,
        .offset = 0,
        .scale = 1,
        .should_interpolate = false,
    }
};

static UdsConfig config = {
    .filename = "mock_config_id3.csv",
    .rows = mockRows,
    .num_rows = 3,
    .car_name = "ID3"
};

TEST_CASE("request full config normal", TAG)
{
    UdsResponse *responses = calloc(config.num_rows, sizeof(UdsResponse));

    TEST_ASSERT_ESP_ERR(ESP_OK, request_full_config(config, responses));
    TEST_ASSERT_ESP_ERR(ESP_OK, responses[0].status);
    TEST_ASSERT_ESP_ERR(ESP_OK, responses[1].status);
    TEST_ASSERT_ESP_ERR(ESP_OK, responses[2].status);
    TEST_ASSERT_EQUAL_STRING("wHvBatDchrg", responses[0].name);
    TEST_ASSERT_EQUAL_STRING("tqEmMechSet", responses[1].name);
    TEST_ASSERT_EQUAL_STRING("stTerm15", responses[2].name);
}

TEST_CASE("datalogger service simulation", TAG)
{
    time_t before = time(NULL);
    start_datalogger_service(&config, DEFAULT_UDS_TIMEOUT_MS, DEFAULT_CAN_TIMEOUT_MS);
    time_t after = time(NULL);

    for (int i=0;i<5;i++) {
        if (get_num_cycles() >= 3) break;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    stop_datalogger_service();

    TEST_ASSERT_EQUAL(0, get_num_timeouts());

    bool raw_found = false;
    char raw_path[128];

    for (time_t t = before; t <= after; t++) {
        struct tm tm;
        localtime_r(&t, &tm);
        snprintf(raw_path, sizeof(raw_path), "/sdcard/logs/raw_id3_%04d-%02d-%02d_%02d-%02d-%02d.log",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        if (file_exists(raw_path)) {
            raw_found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(raw_found, "raw logfile was not found");

    bool csv_found = false;
    char csv_path[128];

    for (time_t t = before; t <= after; t++) {
        struct tm tm;
        localtime_r(&t, &tm);
        snprintf(csv_path, sizeof(csv_path), "/sdcard/logs/log_id3_%04d-%02d-%02d_%02d-%02d-%02d.csv",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        if (file_exists(csv_path)) {
            csv_found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(csv_found, "csv logfile was not found");
}

TEST_CASE("pass NULL to check active ignition", TAG)
{
    TEST_ASSERT_FALSE(uds_check_active_ignition(config.rows[2], 1, 1, NULL));
}

TEST_CASE("Pass NULL to request full config", TAG)
{
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, request_full_config(config, NULL));
}