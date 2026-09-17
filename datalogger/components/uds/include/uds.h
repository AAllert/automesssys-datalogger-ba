#pragma once

// system includes
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include "esp_err.h"

#define MAX_UDS_RESPONSE_SIZE 256 // Match ISOTP buffer size
#define NAME_MAX_LENGTH 31

/**
 * @brief Structure to hold UDS response data
 *
 * Fields:
 * - data
 * - size
 * - status
 * - name from config for reference
 * - timestamp when the response was received
 *
 * @note data/size hold the raw response bytes as received (SID + service-specific payload).
 */
typedef struct
{
    uint8_t data[MAX_UDS_RESPONSE_SIZE];    // allocated memory, where the response data can be written to.
    uint16_t size;                          // size of the received response data
    esp_err_t status;                       // the error code status of the response
    char name[NAME_MAX_LENGTH + 1];         // optional, name from config for reference (not set by send_uds)
    int64_t timestamp_us;                   // Unix timestamp when the response was received, in microseconds
} UdsResponse;

/**
 * @brief Sends one uds message over iso-tp and can to the car.
 *
 * This method is at the top of the can-stack and abstracts the entire process of querying a value from the car.
 * It requires values from the config.csv file, which is individual for each car.
 * The function only sends one ISO-TP request/response transaction at a time, it is not threadsafe.
 *
 * @param[in] canId Destination address for the control device that provides the value to be queried. Column in the config.csv: "numIdTstr"; Example: 402391158
 * @param[in] serviceId UDS-Service Id, used to request a specific UDS service. Column in the config.csv- file: numSid; Example: 34
 * @param[in] requestParams The service-specific request parameters sent after the SID, e.g. DID for service ReadDataByIdentifier (0x22)
 * @param[in] requestParamsSize Number of bytes in requestParams
 * @param[in] responseCanId CAN ID of the device from which the response is expected. If the response comes from another device, it is discarded. Column in the config.csv: numIdGtwy; Example: 402522230
 * @param[in] uds_timeout_ms Overall timeout for the whole request/response transaction, in milliseconds.
 * @param[in] can_timeout_ms The timeout for receiving a single CAN frame
 * @param[out] response Encapsulates the received result
 *
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_ARG if there is an issue with the passed parameters
 * @return - ESP_ERR_INVALID_STATE if there is an isotp request currently running
 * @return - ESP_ERR_NO_MEM on buffer overflow
 * @return - ESP_ERR_TIMEOUT if no valid response was received within timeout_ms
 * @return - ESP_FAIL if an unexpected error occurs
 */
esp_err_t send_uds(const uint32_t canId, const uint32_t serviceId, const uint8_t *requestParams, const uint8_t requestParamsSize, 
                    const uint32_t responseCanId, const uint32_t uds_timeout_ms, const uint32_t can_timeout_ms, UdsResponse *response);
