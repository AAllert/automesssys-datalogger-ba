// tests
#include "can.h"

// system includes
#include "unity.h"
#include "esp_err.h"

// project inlcudes
#include "./twai_selftest.h"
#include "test_helper.h"

TEST_CASE("twai hardware self test", "[twai]")
{
    TEST_ASSERT_ESP_ERR(ESP_OK, twai_selftest());
}
