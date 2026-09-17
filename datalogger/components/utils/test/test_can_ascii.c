// tests
#include "can_ascii.h"

// system includes
#include <stdbool.h>
#include "unity.h"
#include "string.h"
#include "esp_err.h"

// project includes
#include "test_helper.h"

#define TAG "[can_ascii]"

TEST_CASE("parse full can frame ascii normal", TAG)
{
    can_frame_t frame;
    TEST_ASSERT_ESP_ERR(ESP_OK, ascii_to_can_frame("CAN 7DF 8 22 2C 96 00 00 00 00 00", &frame));
    TEST_ASSERT_EQUAL_HEX16(0x7DF, frame.id);
    TEST_ASSERT_EQUAL_HEX8(0x22, frame.data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x2c, frame.data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x96, frame.data[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, frame.data[3]);
    TEST_ASSERT_EQUAL_HEX8(0x00, frame.data[4]);
    TEST_ASSERT_EQUAL_HEX8(0x00, frame.data[5]);
    TEST_ASSERT_EQUAL_HEX8(0x00, frame.data[6]);
    TEST_ASSERT_EQUAL_HEX8(0x00, frame.data[7]);
}

TEST_CASE("parse short can frame ascii normal", TAG)
{
    char *line = "CAN 7DF 8 22 2C 96 00 00";

    can_frame_t frame;
    TEST_ASSERT_ESP_ERR(ESP_OK, ascii_to_can_frame(line, &frame));
    TEST_ASSERT_EQUAL_HEX16(0x7DF, frame.id);
    TEST_ASSERT_EQUAL_HEX8(0x22, frame.data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x2c, frame.data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x96, frame.data[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, frame.data[3]);
    TEST_ASSERT_EQUAL_HEX8(0x00, frame.data[4]);
    TEST_ASSERT_EQUAL_HEX8(0x00, frame.data[5]);
    TEST_ASSERT_EQUAL_HEX8(0x00, frame.data[6]);
    TEST_ASSERT_EQUAL_HEX8(0x00, frame.data[7]);
}

TEST_CASE("Passing Null paramters to ascii_to_can_frame", TAG)
{
    can_frame_t frame;
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, ascii_to_can_frame(NULL, &frame));

    char *line = "CAN 7DF 8 22 2C 96 00 00";
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, ascii_to_can_frame(line, NULL));
}

TEST_CASE("ascii_to_can_frame without prefix", TAG)
{
    char line[64];
    strncpy(line, "7DF 8 22 2C 96 00 00", sizeof(line));
    can_frame_t frame;
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, ascii_to_can_frame(line, &frame));
}

TEST_CASE("parse incomplete ascii frame", TAG)
{
    can_frame_t frame;
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, ascii_to_can_frame("CAN", &frame));
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, ascii_to_can_frame("CAN 7DF", &frame));
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, ascii_to_can_frame("CAN 7DF 0", &frame));
}

TEST_CASE("ascii_to_can_frame with to large dlc", TAG)
{
    can_frame_t frame;
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_SIZE, ascii_to_can_frame("CAN 7DF 9 22 2C 96 00 00 00 00 00 00", &frame));
}

TEST_CASE("ascii_to_can_frame with too many bytes", TAG)
{
    can_frame_t frame;
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, ascii_to_can_frame("CAN 7DF 8 22 2C 96 00 00 00 00 00 00 00 00", &frame));
}

TEST_CASE("can frame to ascii normal", TAG)
{
    can_frame_t frame = {
        .id = 0x7DF,
        .dlc = 5,
        .data = {0x22, 0x2C, 0x96, 0x00, 0x00}
    };

    char buffer[64] = {0};
    TEST_ASSERT_ESP_ERR(ESP_OK, can_frame_to_ascii(&frame, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("CAN 7DF 5 22 2C 96 00 00", buffer);
}

TEST_CASE("can frame to ascii full frame", TAG)
{
    can_frame_t frame = {
        .id = 0x7DF,
        .dlc = 8,
        .data = {0x22, 0x2C, 0x96, 0x00, 0x00, 0x00, 0x00, 0x00}
    };

    char buffer[64] = {0};
    TEST_ASSERT_ESP_ERR(ESP_OK, can_frame_to_ascii(&frame, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("CAN 7DF 8 22 2C 96 00 00 00 00 00", buffer);
}

TEST_CASE("can frame to ascii with dlc out of range", TAG)
{
    can_frame_t frame = {
        .id = 0x7DF,
        .dlc = 9,
        .data = {0x22, 0x2C, 0x96, 0x00, 0x00, 0x00, 0x00, 0x00}
    };

    char buffer[64] = {0};
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_SIZE, can_frame_to_ascii(&frame, buffer, sizeof(buffer)));
}

TEST_CASE("passing Null argumentscan_frame_to_ascii", TAG)
{
    can_frame_t frame = {
        .id = 0x7DF,
        .dlc = 5,
        .data = {0x22, 0x2C, 0x96, 0x00, 0x00}
    };

    char buffer[64] = {0};
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, can_frame_to_ascii(NULL, buffer, sizeof(buffer)));
    TEST_ASSERT_ESP_ERR(ESP_ERR_INVALID_ARG, can_frame_to_ascii(&frame, NULL, sizeof(buffer)));
}