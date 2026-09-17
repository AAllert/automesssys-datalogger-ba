// tests
#include "csv_log_manager.h"

// system includes
#include "unity.h"
#include "esp_err.h"

// project includes
#include <string.h>
#include "storage.h"
#include "test_helper.h"
#include "uds_config.h"

#define TAG "[csv_log_manager]"

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

static UdsResponse responses[] = {
    {
        .data = {0x62,0x1e,0x32,0x00,0x28,0xce,0x35,0xff,0xd8,0xdd,0x69,0x01,0x0b,0xf1,0x57,0xff,0x06,0xa5,0x4d},
        .size = 19,
        .status = ESP_OK,
        .name = "wHvBatDchrg",
        .timestamp_us = 178792132600000
    },
    {
        .data = {0x62,0x58,0xe1,0x00,0x00,0x00,0x00},
        .size = 7,
        .status = ESP_OK,
        .name = "tqEmMechSet",
        .timestamp_us = 178792132600000
    },
    {
        .data = {0x62,0x02,0xb2,0x80},
        .size = 4,
        .status = ESP_OK,
        .name = "stTerm15",
        .timestamp_us = 178792132600000
    }
};

TEST_CASE("start new logfile creates file", TAG)
{
    storage_delete_file("/sdcard/logs/test_create.csv");
    if (file_exists("/sdcard/logs/test_create.csv")) {
        TEST_IGNORE_MESSAGE("existing file could not be deleted");
    }
    TEST_ASSERT_ESP_ERR(ESP_OK, start_new_csv_log_file("test_create.csv", config));
    close_csv_log_file();
    TEST_ASSERT_TRUE(file_exists("/sdcard/logs/test_create.csv"));
    storage_delete_file("/sdcard/logs/test_create.csv");
}

TEST_CASE("start new logfile creates filename", TAG)
{
    time_t before = time(NULL);
    TEST_ASSERT_ESP_ERR(ESP_OK, start_new_csv_log_file(NULL, config));
    time_t after = time(NULL);
    close_csv_log_file();

    bool found = false;
    char path[128];
    for (time_t t = before; t <= after; t++) {
        struct tm tm;
        localtime_r(&t, &tm);
        snprintf(path, sizeof(path), "/sdcard/logs/log_ID3_%04d-%02d-%02d_%02d-%02d-%02d.csv",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        if (file_exists(path)) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "No logfile with the expected name pattern 'raw_<car>_<date>_<time>.csv' for the current time was found");
    storage_delete_file(path);
}

TEST_CASE("start new logfile creates correct header", TAG)
{
    storage_delete_file("/sdcard/logs/test_header.csv");
    if (file_exists("/sdcard/logs/test_header.csv")) {
        TEST_IGNORE_MESSAGE("existing file could not be deleted");
    }
    TEST_ASSERT_ESP_ERR(ESP_OK, start_new_csv_log_file("test_header.csv", config));
    close_csv_log_file();
    char *data = NULL;
    size_t size = 0;
    storage_read_file("/sdcard/logs/test_header.csv", &data, &size);
    TEST_ASSERT_EQUAL_STRING("timestamp,wHvBatDchrg,tqEmMechSet,stTerm15\n", data);
    free(data);
    storage_delete_file("/sdcard/logs/test_header.csv");
}

TEST_CASE("close logfile closes file", TAG)
{
    if (start_new_csv_log_file("test_close.csv", config) != ESP_OK) TEST_IGNORE_MESSAGE("log for testing could not be created");
    close_csv_log_file();
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_STATE, append_csv_log_row(responses, config));
    storage_delete_file("/sdcard/logs/test_close.csv");
}

TEST_CASE("append log row succeeds woth different datatypes in right format", TAG)
{
    storage_delete_file("/sdcard/logs/test_append1.csv");
    if (file_exists("/sdcard/logs/test_append1.csv")) {
        TEST_IGNORE_MESSAGE("existing file could not be deleted");
    }
    if (start_new_csv_log_file("test_append1.csv", config) != ESP_OK) TEST_IGNORE_MESSAGE("log for testing could not be created");
    TEST_ASSERT_ESP_ERR(ESP_OK, append_csv_log_row(responses, config));
    TEST_ASSERT_ESP_ERR(ESP_OK, append_csv_log_row(responses, config));
    close_csv_log_file();
    char *data = NULL;
    size_t size = 0;
    storage_read_file("/sdcard/logs/test_append1.csv", &data, &size);
    TEST_ASSERT_EQUAL_STRING("timestamp,wHvBatDchrg,tqEmMechSet,stTerm15\n178792132.600000,7779319,0.000000,1\n", data);
    free(data);
    storage_delete_file("/sdcard/logs/test_append1.csv");
}

TEST_CASE("delete existing log", TAG)
{
    start_new_csv_log_file("delete.csv", config);
    close_csv_log_file();
    if (!file_exists("/sdcard/logs/delete.csv")) {
        TEST_IGNORE_MESSAGE("file for testing could not be created");
    }
    TEST_ASSERT_ESP_ERR(ESP_OK, delete_csv_log("delete.csv"));
    TEST_ASSERT_FALSE(file_exists("/sdcard/logs/delete.csv"));
    storage_delete_file("/sdcard/logs/delete.csv");
}

TEST_CASE("delete not existing log", TAG)
{
    storage_delete_file("/sdcard/logs/delete.csv");
    if (file_exists("/sdcard/logs/delete.csv")) {
        TEST_IGNORE_MESSAGE("file for testing could not be deleted");
    }
    TEST_ASSERT_ESP_ERR(ESP_OK, delete_csv_log("delete.csv"));
    TEST_ASSERT_FALSE(file_exists("/sdcard/logs/delete.csv"));
    storage_delete_file("/sdcard/logs/delete.csv");
}

TEST_CASE("pass NULL to delete log", TAG)
{
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, delete_csv_log(NULL));
}

TEST_CASE("pass NULL to append log row", TAG)
{
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, append_csv_log_row(NULL, config));
}

TEST_CASE("list logs correctly", TAG)
{
    storage_delete_file("/sdcard/logs/a.csv");
    if (file_exists("/sdcard/logs/a.csv")) {
        TEST_IGNORE_MESSAGE("existing file could not be deleted");
    }
    size_t limit = 100;
    FileInfo *files = calloc(limit, sizeof(FileInfo));
    if (files == NULL) {
        TEST_IGNORE_MESSAGE("Memory for FileInfos could not be allocated");
    }
    size_t num_files;
    TEST_ASSERT_TRUE_MESSAGE(list_collected_csv_logs(files, 0, limit, &num_files), "Could Not open log directory");
    start_new_csv_log_file("a.csv", config);
    close_csv_log_file();
    size_t num_files2;
    TEST_ASSERT_TRUE_MESSAGE(list_collected_csv_logs(files, 0, limit, &num_files2), "Could Not open log directory");
    char msg[128];
    snprintf(msg, sizeof(msg), "Could not find the right number of log files, Before:%d After %d", num_files, num_files2);
    TEST_ASSERT_TRUE_MESSAGE(((num_files2 - num_files) == 1), msg);
    
    bool file_found = false;
    for (int i=0;i<limit;i++) {
        if (strcmp("a.csv", files[i].name) == 0) {
            file_found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(file_found, "The created test log was not in the returned list");
    free(files);
    storage_delete_file("/sdcard/logs/a.csv");
}