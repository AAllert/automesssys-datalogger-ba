// tests
#include "storage.h" 

// system includes
#include <dirent.h>
#include <inttypes.h>
#include "unity.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// project includes
#include "test_helper.h"
#include "app_config.h"

#define TAG "[sdcard]"

TEST_CASE("sd card is accessable", TAG)
{
    TEST_ASSERT_ESP_ERR(ESP_OK, storage_write_file("/sdcard/test/test.txt", "Hallo SD Karte!"));
    char *data = NULL;
    size_t size = 0;
    TEST_ASSERT_ESP_ERR(ESP_OK, storage_read_file("/sdcard/test/test.txt", &data, &size));
    free(data);
}

TEST_CASE("Check sd card mount point", TAG)
{
    TEST_ASSERT_TRUE_MESSAGE(storage_directory_exists("/sdcard"), "Mount point /sdcard could not be opened");
}

TEST_CASE("filesystem unmounted after deinit", TAG)
{
    char *lyrics = "Over mountains an army far from home\nLeading beasts and men through the could and snow.";
    TEST_ASSERT_ESP_ERR(ESP_OK, storage_write_file("/sdcard/test/mountain.txt", lyrics));
    FILE *file = fopen("/sdcard/test/mountain.txt", "r");
    TEST_ASSERT_NOT_NULL(file);
    fclose(file);

    sdcard_deinit();

    file = fopen("/sdcard/test/mountain.txt", "r");
    TEST_ASSERT_NULL(file);

    TEST_ASSERT_ESP_ERR(ESP_OK, sdcard_init(PIN_NUM_CLK, PIN_NUM_CS, PIN_NUM_MISO, PIN_NUM_MOSI));

    file = fopen("/sdcard/test/mountain.txt", "r");
    TEST_ASSERT_NOT_NULL(file);
    char *data = NULL;
    size_t size = 0;
    TEST_ASSERT_ESP_ERR(ESP_OK, storage_read_file("/sdcard/test/mountain.txt", &data, &size));
    TEST_ASSERT_EQUAL_STRING(lyrics, data);
    free(data);
}

TEST_CASE("get_sdcard_usage plausability check", TAG)
{
    const char *path = "/sdcard/test/usage_test.bin";
    uint64_t free_before;
    uint64_t used_before;
    uint64_t total_before;

    uint64_t free_after_write;
    uint64_t used_after_write;
    uint64_t total_after_write;

    uint64_t free_after_delete;
    uint64_t used_after_delete;
    uint64_t total_after_delete;

    storage_delete_file(path);
    if (file_exists(path)) TEST_IGNORE_MESSAGE("file for testing could not be deleted");

    get_sdcard_usage(&free_before, &used_before, &total_before);
    FILE *file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    static uint8_t buffer[4096] = {0};

    for (size_t i=0;i<256;i++) {
        TEST_ASSERT_EQUAL(sizeof(buffer), fwrite(buffer, 1, sizeof(buffer), file));
    }
    fclose(file);

    get_sdcard_usage(&free_after_write, &used_after_write, &total_after_write);
    TEST_ASSERT_LESS_THAN_UINT64(free_before, free_after_write);
    TEST_ASSERT_GREATER_THAN_UINT64(used_before, used_after_write);

    storage_delete_file(path);
    if (file_exists(path)) TEST_IGNORE_MESSAGE("file for testing could not be deleted");

    get_sdcard_usage(&free_after_delete, &used_after_delete, &total_after_delete);

    TEST_ASSERT_EQUAL_UINT64(free_before, free_after_delete);
    TEST_ASSERT_EQUAL_UINT64(used_before, used_after_delete);
    TEST_ASSERT_EQUAL_UINT64(total_before, total_after_delete);
}

// This test case can be used to check if there is traffic on the spi bus with an oscilloscope
TEST_CASE("while spi", "[while_spi]")
{
    for(int i=0;i<100;i++) {
        storage_write_file("/sdcard/test.txt", "Hallo SD Karte!");
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}