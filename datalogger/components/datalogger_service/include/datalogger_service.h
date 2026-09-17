#pragma once

// system includes
#include "esp_err.h"

// project inludes
#include "uds_config.h"
#include "uds.h"

/**
 * @brief Starts the datalogger service.
 *
 * This will create an asynchronous FreeRTOS Task which will continuously loop over the passed uds config and request the data.
 * The received data will be stored in log files on the sd card.
 *
 * @note Only borrows config for the duration of this call and keeps its own deep copy, which it frees itself once stopped. The caller keeps ownership of config.
 *
 * @param[in] config The UDS configuration which shall be requested.
 * @param[in] uds_timeout_ms The timeout for a whole UDS request in milliseconds
 * @param[in] can_timeout_ms The timeout for receiving a single CAN frame in milliseconds
 *
 * @returns - ESP_OK on success
 * @returns - ESP_ERR_NO_MEM if the internal config copy could not be allocated
 */
esp_err_t start_datalogger_service(const UdsConfig *config, uint32_t uds_timeout_ms, uint32_t can_timeout_ms);

/**
 * @brief Stops the datalogger service. 
 * 
 * @return - ESP_OK on success
 */
esp_err_t stop_datalogger_service(void);

/**
 * @brief Getter for the currenct cylce number, will be 0 if logging is inactive
 * @return the current cycle count
 */
uint32_t get_num_cycles(void);

/**
 * @brief Getter for the timeout count in the current logging process, will be 0 if logging is inactive
 * @return the timeout count
 */
uint32_t get_num_timeouts(void);

/**
 * @brief Iterates over each element in the passed UdsConfig and sends for each element an UDS-Request to the ECU. 
 * 
 * If an error occurs during one request like a timeout, the status will be written to the response but requesting will go further. 
 * 
 * @param[in] config The uds config which should be requested.
 * @param[out] responses An array of UdsResponses, where one element is the response for one row in the passed uds config. The caller must manage the memory.
 * 
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_ARG if there is an error in the passed parameters.
 */
esp_err_t request_full_config(UdsConfig config, UdsResponse *responses);

/**
 * @brief Checks if the ignition is on or not via an UDS request.
 *
 * @param[in] ignition_row The row from the UDS config which holds the configuration for stTerm15.
 * @param[in] uds_timeout The uds timeout after which the request stops.
 * @param[in] can_timeout The can timeout for receiving a single frame.
 * @param[out] response The raw UDS response as receives for further usage.
 *
 * @return - true if a valid response was received and the decoded signal value is non-zero (ignition on)
 * @return - false otherwise (no/negative response, response for the wrong DID, or decoded value is zero == ignition off)
 */
bool uds_check_active_ignition(ConfigRow ignition_row, const uint32_t uds_timeout_ms, const uint32_t can_timeout_ms, UdsResponse *response);
