// tests
#include "uds_config.h"

// system includes
#include "unity.h"

// project includes
#include "test_helper.h"

#define TAG "[uds_config]"

TEST_CASE("did_to_bytes normal", TAG)
{
    uint8_t bytes[2];

    did_to_bytes(0x0000, bytes);

    TEST_ASSERT_EQUAL_HEX8(0x00, bytes[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, bytes[1]);
}

TEST_CASE("did_to_bytes converts values with only high byte set", TAG)
{
    uint8_t bytes[2];

    did_to_bytes(0x1200, bytes);

    TEST_ASSERT_EQUAL_HEX8(0x12, bytes[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, bytes[1]);
}

TEST_CASE("did_to_bytes converts values with only low byte set", TAG)
{
    uint8_t bytes[2];

    did_to_bytes(0x0034, bytes);

    TEST_ASSERT_EQUAL_HEX8(0x00, bytes[0]);
    TEST_ASSERT_EQUAL_HEX8(0x34, bytes[1]);
}

TEST_CASE("did_to_bytes converts maximum DID correctly", TAG)
{
    uint8_t bytes[2];

    did_to_bytes(0xFFFF, bytes);

    TEST_ASSERT_EQUAL_HEX8(0xFF, bytes[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, bytes[1]);
}

TEST_CASE("get_term15_row returns the right config row", TAG)
{
    ConfigRow ignition_row;
    TEST_ASSERT_ESP_ERR(ESP_OK, parse_active_config());
    TEST_ASSERT_ESP_ERR(ESP_OK, get_active_ignition_row(&ignition_row));
    TEST_ASSERT_EQUAL_STRING("stTerm15", ignition_row.name);
}