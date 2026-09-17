// tests 
#include "uds_decoder.h"

// system includes
#include "unity.h"

#define TAG "[uds_decoder]"

#define B(x) S_to_binary_(#x)

static inline unsigned long long S_to_binary_(const char *s)
{
    unsigned long long i = 0;
    while (*s) {
        i <<= 1;
        i += *s++ - '0';
    }
    return i;
}

TEST_CASE("extract first bit big endian", TAG)
{
    uint8_t data[] = {B(10000000)};
    TEST_ASSERT_EQUAL_UINT32(1, extract_bits(data, 7, 1, 0));
}

TEST_CASE("extract first bit little endian", TAG)
{
    uint8_t data[] = {B(11111110)};
    TEST_ASSERT_EQUAL_UINT32(0, extract_bits(data, 0, 1, 1));
}

TEST_CASE("extract full byte big endian", TAG)
{
    uint8_t data[] = {B(10101010)};
    TEST_ASSERT_EQUAL_UINT32(B(10101010), extract_bits(data, 7, 8, 0));
}

TEST_CASE("extract full byte little endian", TAG)
{
    uint8_t data[] = {B(10101010)};
    TEST_ASSERT_EQUAL_UINT32(B(10101010), extract_bits(data, 0, 8, 1));
}

TEST_CASE("extract middle of byte big endian", TAG)
{
    uint8_t data[] = {B(10101010)};
    TEST_ASSERT_EQUAL_UINT32(B(01), extract_bits(data, 2, 2, 0));
}

TEST_CASE("extract middle of byte little endian", TAG)
{
    uint8_t data[] = {B(10101010)};
    TEST_ASSERT_EQUAL_UINT32(B(10), extract_bits(data, 2, 2, 1));
}

TEST_CASE("extract across two bytes big endian", TAG)
{
    uint8_t data[] = {B(00000101), B(01000000)};
    TEST_ASSERT_EQUAL_UINT32(B(101010), extract_bits(data, 2, 6, 0));
}

TEST_CASE("extract across two bytes little endian", TAG)
{
    uint8_t data[] = {B(10000000), B(00001010)};
    TEST_ASSERT_EQUAL_UINT32(B(101010), extract_bits(data, 6, 6, 1));
}

TEST_CASE("extract three bytes full big endian", TAG)
{
    uint8_t data[] = {B(10101010), B(10101010), B(10101010)};
    TEST_ASSERT_EQUAL_UINT32(B(101010101010101010101010), extract_bits(data, 7, 24, 0));
}

TEST_CASE("extract three bytes full little endian", TAG)
{
    uint8_t data[] = {B(10101010), B(10101010), B(10101010)};
    TEST_ASSERT_EQUAL_UINT32(B(101010101010101010101010), extract_bits(data, 0, 24, 1));
}

TEST_CASE("extract tClnt", TAG)
{
    uint8_t data[] = {0x42};
    TEST_ASSERT_EQUAL_UINT32(66, extract_bits(data, 7, 8, 0));
}

TEST_CASE("decode tClnt", TAG)
{
    TEST_ASSERT_EQUAL_UINT32(66, decode_raw_value(66, 1, 0));
}

TEST_CASE("extract wRail1", TAG)
{
    uint8_t data[] = {0x00, 0x16, 0xc2};
    TEST_ASSERT_EQUAL_UINT32(5826, extract_bits(data, 7, 24, 0));
}

TEST_CASE("decode wRail1", TAG)
{
    TEST_ASSERT_EQUAL_DOUBLE(582.6, decode_raw_value(5826, 10, 0));
}

TEST_CASE("decode_raw_float32_value reinterprets bits instead of scaling", TAG)
{
    TEST_ASSERT_EQUAL_DOUBLE(1.5, decode_raw_float32_value(0x3FC00000));
}

TEST_CASE("decode_raw_float32_value on real tClntEdcIn payload", TAG)
{
    TEST_ASSERT_EQUAL_DOUBLE_MESSAGE(24.6572265625, decode_raw_float32_value(1103446528),
        "raw uint32 must be reinterpreted as IEEE-754 float32, not passed through as a plain number");
}

TEST_CASE("uds_is_negative_response detects a negative response and extracts the NRC", TAG)
{
    uint8_t data[] = {0x7F, 0x22, 0x31};
    uint8_t nrc = 0;
    TEST_ASSERT_TRUE(uds_is_negative_response(data, sizeof(data), &nrc));
    TEST_ASSERT_EQUAL_HEX8(0x31, nrc);
}

TEST_CASE("uds_is_negative_response returns false for a positive response", TAG)
{
    uint8_t data[] = {0x62, 0xF4, 0x0D, 0x01};
    TEST_ASSERT_FALSE(uds_is_negative_response(data, sizeof(data), NULL));
}

TEST_CASE("uds_is_negative_response reports NRC 0 when the response is too short to contain one", TAG)
{
    uint8_t data[] = {0x7F};
    uint8_t nrc = 0xFF;
    TEST_ASSERT_TRUE(uds_is_negative_response(data, sizeof(data), &nrc));
    TEST_ASSERT_EQUAL_HEX8(0, nrc);
}

TEST_CASE("uds_is_negative_response returns false for NULL/empty input", TAG)
{
    uint8_t data[] = {0x7F, 0x22, 0x31};
    TEST_ASSERT_FALSE(uds_is_negative_response(NULL, 3, NULL));
    TEST_ASSERT_FALSE(uds_is_negative_response(data, 0, NULL));
}

TEST_CASE("uds_extract_rdbi_payload extracts the signal payload of a positive response", TAG)
{
    uint8_t data[] = {0x62, 0xF4, 0x0D, 0xAA, 0xBB, 0xCC};
    const uint8_t *payload = NULL;
    size_t payload_size = 0;

    esp_err_t err = uds_extract_rdbi_payload(data, sizeof(data), 0xF40D, &payload, &payload_size);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_PTR(data + 3, payload);
    TEST_ASSERT_EQUAL(3, payload_size);
    TEST_ASSERT_EQUAL_HEX8(0xAA, payload[0]);
}

TEST_CASE("uds_extract_rdbi_payload rejects a negative response", TAG)
{
    uint8_t data[] = {0x7F, 0x22, 0x31};
    const uint8_t *payload = NULL;
    size_t payload_size = 0;

    esp_err_t err = uds_extract_rdbi_payload(data, sizeof(data), 0xF40D, &payload, &payload_size);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);
}

TEST_CASE("uds_extract_rdbi_payload rejects a DID mismatch", TAG)
{
    uint8_t data[] = {0x62, 0xF4, 0x0D, 0xAA};
    const uint8_t *payload = NULL;
    size_t payload_size = 0;

    esp_err_t err = uds_extract_rdbi_payload(data, sizeof(data), 0x1234, &payload, &payload_size);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);
}

TEST_CASE("uds_extract_rdbi_payload rejects a response shorter than the RDBI header", TAG)
{
    uint8_t data[] = {0x62, 0xF4};
    const uint8_t *payload = NULL;
    size_t payload_size = 0;

    esp_err_t err = uds_extract_rdbi_payload(data, sizeof(data), 0xF40D, &payload, &payload_size);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);
}

TEST_CASE("uds_extract_rdbi_payload handles a response with no signal bytes", TAG)
{
    uint8_t data[] = {0x62, 0xF4, 0x0D};
    const uint8_t *payload = NULL;
    size_t payload_size = 999;

    esp_err_t err = uds_extract_rdbi_payload(data, sizeof(data), 0xF40D, &payload, &payload_size);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_PTR(data + 3, payload);
    TEST_ASSERT_EQUAL(0, payload_size);
}

TEST_CASE("uds_extract_rdbi_payload rejects NULL arguments", TAG)
{
    uint8_t data[] = {0x62, 0xF4, 0x0D, 0xAA};
    const uint8_t *payload = NULL;
    size_t payload_size = 0;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, uds_extract_rdbi_payload(NULL, 4, 0xF40D, &payload, &payload_size));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, uds_extract_rdbi_payload(data, 4, 0xF40D, NULL, &payload_size));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, uds_extract_rdbi_payload(data, 4, 0xF40D, &payload, NULL));
}