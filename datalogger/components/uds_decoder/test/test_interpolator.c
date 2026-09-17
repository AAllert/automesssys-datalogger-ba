// tests
#include "interpolator.h"

// system includes
#include "unity.h"

#define TAG "[interpolator]"

TEST_CASE("linear interpolation normal", TAG)
{
    TEST_ASSERT_EQUAL_DOUBLE(3, linear_interpolate(5, 5, 1, 1, 3));
}

TEST_CASE("linear interpolation divide by zero", TAG)
{
    TEST_ASSERT_EQUAL_DOUBLE(2, linear_interpolate(0, 2, 0, 1, 2));
}

TEST_CASE("linear interpolation with target time outside range", TAG)
{
    TEST_ASSERT_EQUAL_DOUBLE(10, linear_interpolate(5, 5, 1, 1, 10));
}

TEST_CASE("set and get target timestamp", TAG)
{
    interpolator_set_target_timestamp(1786519601);
    TEST_ASSERT_EQUAL_INT64(1786519601, interpolator_get_target_timestamp());
}

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

TEST_CASE("interpolate session simulation", TAG)
{
    interpolator_start_session(config);
    double result = 0.0;
    TEST_ASSERT_FALSE_MESSAGE(interpolate("wHvBatDchrg", 10, 10.0, &result), "interpolate wHvBatDchrg was true");
    TEST_ASSERT_EQUAL(0.0, result);

    TEST_ASSERT_FALSE_MESSAGE(interpolate("tqEmMechSet", 12, 12.0, &result), "interpolate tqEmMechSet was true");
    TEST_ASSERT_EQUAL(0.0, result);

    TEST_ASSERT_FALSE_MESSAGE(interpolate("stTerm15", 13, 0.0, &result), "interpolate stTerm15 was true");
    TEST_ASSERT_EQUAL(0.0, result);

    interpolator_set_target_timestamp(13);

    TEST_ASSERT_TRUE_MESSAGE(interpolate("wHvBatDchrg", 15, 15.0, &result), "interpolate wHvBatDchrg was false");
    TEST_ASSERT_EQUAL(13.0, result);

    TEST_ASSERT_TRUE_MESSAGE(interpolate("tqEmMechSet", 16, 16.0, &result), "interpolate tqEmMechSet was false");
    TEST_ASSERT_EQUAL(13.0, result);

    TEST_ASSERT_TRUE_MESSAGE(interpolate("stTerm15", 19, 1.0, &result), "interpolate stTerm15 was false");
    TEST_ASSERT_EQUAL(0.0, result);
}

TEST_CASE("restart interpolation session", TAG)
{
    interpolator_start_session(config);
    double result = 0.0;
    TEST_ASSERT_FALSE_MESSAGE(interpolate("wHvBatDchrg", 10, 10.0, &result), "interpolate wHvBatDchrg was true");
    TEST_ASSERT_EQUAL(0.0, result);

    interpolator_start_session(config);
    TEST_ASSERT_FALSE_MESSAGE(interpolate("wHvBatDchrg", 10, 10.0, &result), "interpolate wHvBatDchrg was true");
    TEST_ASSERT_EQUAL(0.0, result);
}

TEST_CASE("get timestamp before end of first iteration", TAG)
{
    interpolator_set_target_timestamp(100);
    interpolator_start_session(config);
    TEST_ASSERT_EQUAL(0, interpolator_get_target_timestamp());
}