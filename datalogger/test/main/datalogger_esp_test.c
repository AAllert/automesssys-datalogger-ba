// system includes
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_app_trace.h"
#include "string.h"

// project includes
#include "app_config.h"
#include "can_backend.h"
#include "usb_serial.h"
#include "storage.h"
#include "nvs_storage.h"
#include "deepsleep_handler.h"
#include "event_loop.h"

#define TAG "TEST"
#define MAX_TAGS 16
#define MAX_TAG_LEN 32

static char s_test_tags[MAX_TAGS][MAX_TAG_LEN];
static int  s_test_tag_count = 0;
static bool s_run_all        = false;
static bool s_coverage       = false;

static void print_banner(const char *text);
static void receive_runner_config(void);
static void parse_tags(const char *tags_str);

void app_main(void)
{
    // USB serial must be up first: the handshake runs over it regardless of which CAN backend is later selected.
    usb_serial_init();
    event_bus_init();
    deepsleep_handler_init(PIN_NUM_MOSFET, PIN_NUM_PIR);
    deepsleep_activate_peripherals();

    // initilize storage
    esp_err_t err = sdcard_init(PIN_NUM_CLK, PIN_NUM_CS, PIN_NUM_MISO, PIN_NUM_MOSI);
    ESP_LOGI("SD Card Init", "sdcard_init returned with error: %s", esp_err_to_name(err));
    spiffs_init(MAX_SPIFFS_FILES);
    nvs_init();

    storage_delete_directory("/sdcard/test", true);

    // Receive BACKEND and TAGS from the testrunner, then acknowledge.
    receive_runner_config();

    // Initialize whichever backend was selected during the handshake.
    can_backend_init();

    print_banner("Running selected tests");

    UNITY_BEGIN();
    if (s_run_all) {
        unity_run_all_tests();
    } else {
        for (int i = 0; i < s_test_tag_count; i++) {
            unity_run_tests_by_tag(s_test_tags[i], false);
        }
    }
    UNITY_END();

    storage_delete_directory("/sdcard/test", true);

    if (s_coverage) {
        ESP_LOGI(TAG, "Waiting for OpenOCD for coverage dump...");
        esp_gcov_dump();
        ESP_LOGI(TAG, "coverage dump completed");
    }
}

// config reception

static void receive_runner_config(void)
{
    char line[256];

    // Step 1: HELLO / READY handshake
    ESP_LOGI(TAG, "Waiting for testrunner...");
    while (1) {
        usb_serial_write_line("HELLO");
        if (usb_serial_read_line(line, sizeof(line), 1000) == ESP_OK) {
            if (strcmp(line, "READY") == 0) {
                ESP_LOGI(TAG, "Runner ready");
                break;
            }
        }
        ESP_LOGI(TAG, "Retrying handshake...");
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // Step 2: BACKEND:<usb|can>
    while (usb_serial_read_line(line, sizeof(line), 5000) != ESP_OK) {
        ESP_LOGW(TAG, "Waiting for BACKEND config...");
    }
    if (strncmp(line, "BACKEND:usb", 11) == 0) {
        can_backend_set_type(CAN_BACKEND_UART);
    } else {
        can_backend_set_type(CAN_BACKEND_CAN);
    }

    // Step 3: TAGS:<ALL|[tag1],[tag2],...>
    while (usb_serial_read_line(line, sizeof(line), 5000) != ESP_OK) {
        ESP_LOGW(TAG, "Waiting for TAGS config...");
    }
    if (strncmp(line, "TAGS:", 5) == 0) {
        parse_tags(line + 5);
    }

    // Step 4: COVERAGE:<0|1> — if covereage is set execute the gcov dump, otherwise the call will block forever. 
    while (usb_serial_read_line(line, sizeof(line), 5000) != ESP_OK) {
        ESP_LOGW(TAG, "Waiting for COVERAGE config...");
    }
    s_coverage = (strncmp(line, "COVERAGE:1", 10) == 0);

    usb_serial_write_line("CONFIG_ACK");
    ESP_LOGI(TAG, "Config acknowledged — backend and tags set");
}

static void parse_tags(const char *tags_str)
{
    // Parse comma-separated list: "[can_backend],[uds]"
    char buf[256];
    strncpy(buf, tags_str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok = strtok(buf, ",");
    while (tok && s_test_tag_count < MAX_TAGS) {
        strncpy(s_test_tags[s_test_tag_count], tok, MAX_TAG_LEN - 1);
        s_test_tags[s_test_tag_count][MAX_TAG_LEN - 1] = '\0';
        ESP_LOGI(TAG, "Tag[%d]: %s", s_test_tag_count, s_test_tags[s_test_tag_count]);
        s_test_tag_count++;
        tok = strtok(NULL, ",");
    }
}

// Helpers

static void print_banner(const char *text)
{
    printf("\n##### %s #####\n\n", text);
}
