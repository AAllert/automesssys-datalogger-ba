#pragma once

// system includes
#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#define UDS_RDBI_POSITIVE_RESPONSE_SID 0x62
#define UDS_RDBI_RESPONSE_HEADER_SIZE 3
#define UDS_NEGATIVE_RESPONSE_SID 0x7F
#define UDS_NRC_RESPONSE_PENDING 0x78

/**
 * @brief Extracts the relevant bits from the raw bytes received via uds. 
 * 
 * This needs the payload of the response and the dbc string from the config. 
 * 
 * @param[in] data a byte array of the received data, this is only the payload without sid and did. 
 * @param[in] startbit dbc start bit, the bit at which the relevant data starts
 * @param[in] length The number of bits which are relevant after the start bit
 * @param[in] byte_order 0 for big endian (motorola), 1 for little endian (intel)
 * 
 * @note The caller must ensure startbit/8 < data_size before calling, otherwise it could cause undefined behavior. 
 * 
 * @return The extracted data
 */
uint32_t extract_bits(const uint8_t *data, uint16_t startBit, uint8_t length, uint8_t byte_order);

/**
 * @brief Decodes the received raw value to the real physical value.
 *
 * @param[in] raw_value The raw value which was extraced from the received uds response.
 * @param[in] scale the factor the data was multiplicated with, column numFac in the uds_config
 * @param[in] offset The offset added to the raw value, column numOfs in the uds_config
 *
 * @return The physical value
 */
double decode_raw_value(const uint32_t raw_value, const double scale, const double offset);

/**
 * @brief Decodes a raw float value.
 *
 * @param[in] raw_value The raw value which was extracted from the received uds response.
 *
 * @return The physical value
 */
double decode_raw_float32_value(const uint32_t raw_value);

/**
 * @brief Checks whether a UDS response is a negative response (SID 0x7F).
 *
 * @param[in] response_data Raw response bytes as received via send_uds.
 * @param[in] response_size Number of valid bytes in response_data.
 * @param[out] nrc_out If non-NULL, receives the negative response code.
 *
 * @return true if response_data is a negative response, false otherwise.
 */
bool uds_is_negative_response(const uint8_t *response_data, uint16_t response_size, uint8_t *nrc_out);

/**
 * @brief Extracts the signal payload from a positive ReadDataByIdentifier (SID 0x22) response.
 *
 * @param[in] response_data Raw response bytes as received via send_uds.
 * @param[in] response_size Number of valid bytes in response_data.
 * @param[in] expected_did The DID that was requested; validated against the DID echoed by the response.
 * @param[out] payload Receives a pointer into response_data, positioned after the RDBI header. No copy is made.
 * @param[out] payload_size Receives the number of signal bytes available at *payload.
 *
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_ARG if response_data, payload or payload_size is NULL
 * @return - ESP_ERR_INVALID_SIZE if response_size is smaller than the RDBI header size
 * @return - ESP_ERR_INVALID_RESPONSE if the response is not a positive RDBI response for the expected_did
 */
esp_err_t uds_extract_rdbi_payload(const uint8_t *response_data, uint16_t response_size, uint16_t expected_did, const uint8_t **payload, size_t *payload_size);
