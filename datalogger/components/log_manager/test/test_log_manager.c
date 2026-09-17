// tests
#include "log_manager.h"

// system includes
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "unity.h"
#include "esp_err.h"

// project includes
#include "test_helper.h"
#include "storage.h"

#define TAG "[log_manager]"

TEST_CASE("open log creates file", TAG)
{
    storage_delete_file("/sdcard/logs/toller_log.log");
    if (file_exists("/sdcard/logs/toller_log.log")) {
        TEST_IGNORE_MESSAGE("existing file could not be deleted");
    }
    TEST_ASSERT_ESP_ERR(ESP_OK, start_new_log_file("toller_log.log", NULL));
    close_log_file();
    TEST_ASSERT_TRUE(file_exists("/sdcard/logs/toller_log.log"));
    storage_delete_file("/sdcard/logs/toller_log.log");
}

TEST_CASE("open log creates filename", TAG)
{
    time_t before = time(NULL);
    TEST_ASSERT_ESP_ERR(ESP_OK, start_new_log_file(NULL, "id3"));
    time_t after = time(NULL);
    close_log_file();

    bool found = false;
    char path[128];
    for (time_t t = before; t <= after; t++) {
        struct tm tm;
        localtime_r(&t, &tm);
        snprintf(path, sizeof(path), "/sdcard/logs/raw_id3_%04d-%02d-%02d_%02d-%02d-%02d.log",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        if (file_exists(path)) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "No logfile with the expected name pattern 'raw_<car>_<date>_<time>.log' for the current time was found");
    storage_delete_file(path);
}

TEST_CASE("close_log_file closes file", TAG)
{
    start_new_log_file("close_test.log", NULL);
    close_log_file();
    LogEntry entry = {
        .timestamp_us = 1719936000000000LL,
        .name = "vVeh1",
        .data_length = 3,
        .data = {0x12, 0x34, 0x56}
    };
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_STATE, append_to_log(&entry));
    storage_delete_file("/sdcard/logs/close_test.log");
}

TEST_CASE("append_to_log succeeds in right format", TAG)
{
    storage_delete_file("/sdcard/logs/write_test.log");
    if (file_exists("/sdcard/logs/write_test.log")) {
        TEST_IGNORE_MESSAGE("existing file could not be deleted");
    }
    start_new_log_file("write_test.log", NULL);
    LogEntry entry = {
        .timestamp_us = 1719936000000000LL,
        .name = "vVeh1",
        .data_length = 3,
        .data = {0x12, 0x34, 0x56}
    };
    TEST_ASSERT_ESP_ERR(ESP_OK, append_to_log(&entry));
    TEST_ASSERT_ESP_ERR(ESP_OK, append_to_log(&entry));
    close_log_file();
    char *data = NULL;
    size_t size = 0;
    storage_read_file("/sdcard/logs/write_test.log", &data, &size);
    TEST_ASSERT_EQUAL_STRING("1719936000.000000 - vVeh1 - b'12,34,56'\n1719936000.000000 - vVeh1 - b'12,34,56'\n", data);
    free(data);
    storage_delete_file("/sdcard/logs/write_test.log");
}

TEST_CASE("delete existing log", TAG)
{
    start_new_log_file("delete.log", NULL);
    close_log_file();
    if (!file_exists("/sdcard/logs/delete.log")) {
        TEST_IGNORE_MESSAGE("file for testing could not be created");
    }
    TEST_ASSERT_ESP_ERR(ESP_OK, delete_log("delete.log"));
    TEST_ASSERT_FALSE(file_exists("/sdcard/logs/delete.log"));
    storage_delete_file("/sdcard/logs/delete.log");
}

TEST_CASE("delete not existing log", TAG)
{
    storage_delete_file("/sdcard/logs/delete.log");
    if (file_exists("/sdcard/logs/delete.log")) {
        TEST_IGNORE_MESSAGE("file for testing could not be deleted");
    }
    TEST_ASSERT_ESP_ERR(ESP_OK, delete_log("delete.log"));
    TEST_ASSERT_FALSE(file_exists("/sdcard/logs/delete.log"));
}

TEST_CASE("pass double Null to start_new_logfile", TAG)
{
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, start_new_log_file(NULL, NULL));
}

TEST_CASE("pass Null to delete_log", TAG)
{
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, delete_log(NULL));
}

TEST_CASE("pass Null to append_log", TAG)
{
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, append_to_log(NULL));
}

TEST_CASE("list logs correctly", TAG)
{
    storage_delete_file("/sdcard/logs/a.log");
    if (file_exists("/sdcard/logs/a.log")) {
        TEST_IGNORE_MESSAGE("existing file could not be deleted");
    }
    size_t limit = 100;
    FileInfo *files = calloc(limit, sizeof(FileInfo));
    if (files == NULL) {
        TEST_IGNORE_MESSAGE("Memory for FileInfos could not be allocated");
    }
    size_t num_files;
    TEST_ASSERT_TRUE_MESSAGE(list_collected_logs(files, 0, limit, &num_files), "Could Not open log directory");
    start_new_log_file("a.log", NULL);
    close_log_file();
    size_t num_files2;
    TEST_ASSERT_TRUE_MESSAGE(list_collected_logs(files, 0, limit, &num_files2), "Could Not open log directory");
    char msg[128];
    snprintf(msg, sizeof(msg), "Could not find the right number of log files, Before:%d After %d", num_files, num_files2);
    TEST_ASSERT_TRUE_MESSAGE(((num_files2 - num_files) == 1), msg);
    
    bool file_found = false;
    for (int i=0;i<limit;i++) {
        if (strcmp("a.log", files[i].name) == 0) {
            file_found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(file_found, "The created test log was not in the returned list");
    free(files);
    storage_delete_file("/sdcard/logs/a.log");
}