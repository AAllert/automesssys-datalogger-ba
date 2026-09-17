// tests
#include "nvs_storage.h"

// system includes
#include <stdlib.h>
#include "unity.h"
#include "esp_err.h"
#include "esp_log.h"

// project inlcudes
#include "test_helper.h"

#define TAG "[nvs_storage]"

TEST_CASE("save and load active config", TAG)
{
    TEST_ASSERT_ESP_ERR(ESP_OK, nvs_set_active_config("Uds_test_config.csv"));
    char *filename;
    TEST_ASSERT_ESP_ERR(ESP_OK, nvs_get_active_config(&filename));
    TEST_ASSERT_EQUAL_STRING("Uds_test_config.csv", filename);
    free(filename);
}

TEST_CASE("try load active config which is not set", TAG)
{
    if (erase_key("uds", "active_config") != ESP_OK ) {
        TEST_IGNORE_MESSAGE("key could not be erased");
    }
    char *filename;
    esp_err_t result = nvs_get_active_config(&filename);
    ESP_LOGI("nvs", "nvs load not set config: %s", filename ? filename : "(null)");
    free(filename);
    TEST_ASSERT_ESP_ERR(ESP_ERR_NVS_NOT_FOUND, result);
}

static void assert_save_and_load_u32(esp_err_t (*get)(uint32_t *), esp_err_t (*set)(uint32_t))
{
    uint32_t original = 0;
    esp_err_t result = get(&original);
    TEST_ASSERT_TRUE(result == ESP_OK || result == ESP_ERR_NVS_NOT_FOUND);
    TEST_ASSERT_ESP_ERR(ESP_OK, set(original + 100));
    uint32_t loaded;
    TEST_ASSERT_ESP_ERR(ESP_OK, get(&loaded));
    TEST_ASSERT_EQUAL(original + 100, loaded);
    TEST_ASSERT_ESP_ERR(ESP_OK, set(original));
}

static void assert_default_when_not_set(esp_err_t (*get)(uint32_t *), esp_err_t (*set)(uint32_t), const char *nvs_namespace, const char *key)
{
    uint32_t original = 0;
    esp_err_t result = get(&original);
    TEST_ASSERT_TRUE(result == ESP_OK || result == ESP_ERR_NVS_NOT_FOUND);
    if (erase_key(nvs_namespace, key) != ESP_OK) {
        TEST_IGNORE_MESSAGE("key could not be erased");
    }
    uint32_t loaded;
    TEST_ASSERT_ESP_ERR(ESP_OK, get(&loaded));
    TEST_ASSERT_ESP_ERR(ESP_OK, set(original));
}

TEST_CASE("save and load can timeout", TAG)
{
    assert_save_and_load_u32(nvs_get_can_timeout, nvs_set_can_timeout);
}

TEST_CASE("try to load can timeout which is not set", TAG)
{
    assert_default_when_not_set(nvs_get_can_timeout, nvs_set_can_timeout, "settings", "can_timeout");
}

TEST_CASE("save and load uds timeout", TAG)
{
    assert_save_and_load_u32(nvs_get_uds_timeout, nvs_set_uds_timeout);
}

TEST_CASE("try to load uds timeout which is not set", TAG)
{
    assert_default_when_not_set(nvs_get_uds_timeout, nvs_set_uds_timeout, "settings", "uds_timeout");
}

TEST_CASE("save and load deepsleep timeout", TAG)
{
    assert_save_and_load_u32(nvs_get_deepsleep_timeout, nvs_set_deepsleep_timeout);
}

TEST_CASE("try to load deepsleep timeout which is not set", TAG)
{
    assert_default_when_not_set(nvs_get_deepsleep_timeout, nvs_set_deepsleep_timeout, "settings", "sleep_timeout");
}

TEST_CASE("save and load term15 request interval", TAG)
{
    assert_save_and_load_u32(nvs_get_term15_request_interval, nvs_set_term15_request_interval);
}

TEST_CASE("try to load term15 request interval which is not set", TAG)
{
    assert_default_when_not_set(nvs_get_term15_request_interval, nvs_set_term15_request_interval, "settings", "term15_interval");
}

TEST_CASE("nvs double init", TAG)
{
    TEST_ASSERT_ESP_ERR(ESP_OK, nvs_init());
    TEST_ASSERT_ESP_ERR(ESP_OK, nvs_init());
}
