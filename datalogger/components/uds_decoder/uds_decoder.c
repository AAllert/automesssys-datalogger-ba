// implements
#include "uds_decoder.h"

// system includes
#include <inttypes.h>
#include <string.h>

static uint32_t extract_motorola_bits(const uint8_t *data, uint16_t startbit, uint8_t length)
{
    uint32_t value = 0;
    uint16_t bit = startbit;

    for (uint8_t i = 0; i < length; i++) {
        uint16_t byte = bit / 8;
        uint8_t bit_in_byte = bit % 8;
        value <<= 1;
        value |= (data[byte] >> bit_in_byte) & 1;

        if (bit_in_byte == 0) {
            bit += 15;  // jump to next byte
        } else {
            bit--;
        }
    }

    return value;
}

static uint32_t extract_intel_bits(const uint8_t *data, uint16_t startbit, uint8_t length)
{
    uint32_t value = 0;
    for (uint8_t i = 0; i < length; i++) {
        uint16_t bit_index = startbit + i;
        uint16_t byte_index = bit_index / 8;
        uint8_t bit_in_byte = bit_index % 8;
        value |= ((uint32_t)((data[byte_index] >> bit_in_byte) & 0x1)) << i;
    }
    return value;
}

uint32_t extract_bits(const uint8_t *data, uint16_t startBit, uint8_t length, uint8_t byte_order) {
    if (byte_order == 0) return extract_motorola_bits(data, startBit, length);
    return extract_intel_bits(data, startBit, length);
}

double decode_raw_value(const uint32_t raw_value, const double scale, const double offset) {
    return ((double)raw_value / scale) - offset;
}

double decode_raw_float32_value(const uint32_t raw_value) {
    float value;
    memcpy(&value, &raw_value, sizeof(value));
    return (double)value;
}

bool uds_is_negative_response(const uint8_t *response_data, uint16_t response_size, uint8_t *nrc_out) {
    if (!response_data || response_size == 0 || response_data[0] != UDS_NEGATIVE_RESPONSE_SID) {
        return false;
    }
    if (nrc_out) {
        *nrc_out = (response_size > 2) ? response_data[2] : 0;
    }
    return true;
}

esp_err_t uds_extract_rdbi_payload(const uint8_t *response_data, uint16_t response_size, uint16_t expected_did, const uint8_t **payload, size_t *payload_size) {
    if (!response_data || !payload || !payload_size) return ESP_ERR_INVALID_ARG;
    if (response_size < UDS_RDBI_RESPONSE_HEADER_SIZE) return ESP_ERR_INVALID_SIZE;

    uint16_t echoed_did = ((uint16_t)response_data[1] << 8) | response_data[2];
    if (response_data[0] != UDS_RDBI_POSITIVE_RESPONSE_SID || echoed_did != expected_did) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    *payload = response_data + UDS_RDBI_RESPONSE_HEADER_SIZE;
    *payload_size = (size_t)(response_size - UDS_RDBI_RESPONSE_HEADER_SIZE);
    return ESP_OK;
}