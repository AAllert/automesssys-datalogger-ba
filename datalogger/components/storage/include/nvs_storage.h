#pragma once

// system includes
#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Initilizes the non-volatile storage to store key-value pairs.
 * 
 * The nvs is used to store key value pairs persistently in flash. 
 * 
 * @return - ESP_OK on successs
 * 
 * @note To access values use the other functions from this header, manual access works but is not recommended.
 */
esp_err_t nvs_init(void);

/**
 * @brief Erases a key and its value from the nvs storage.
 *
 * @param[in] namespace the namespace the key belongs to
 * @param[in] key the key to delete
 *
 * @return ESP_OK if the key was successfully erased or did not exist before
 * @return ESP_FAIL if an error occurs
 */
esp_err_t erase_key(const char *namespace, const char *key);

/**
 * @brief Updates the active config in nvs. 
 * 
 * @param[in] filename The filename of the config which should be stored as the active one. 
 * 
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_ARG if filename is NULL
 * @return - ESP_FAIL if an error occurs
 */
esp_err_t nvs_set_active_config(const char *filename);

/**
 * @brief Loads the filename of the active config from nvs. 
 * 
 * @param[out] filename The filename which was stored in
 *
 * @note The caller must free the memory of the filename. Set to NULL if the config could not be loaded.
 *
 * @return - ESP_OK on success
 * @return - ESP_ERR_NVS_NOT_FOUND if the value has not yet been set
 * @return - ESP_ERR_INVALID_ARG if filename is NULL
 * @return - ESP_FAIL if an error occurs
 */
esp_err_t nvs_get_active_config(char **filename);

/**
 * @brief Updates the CAN receive timeout in nvs.
 *
 * @param[in] timeout_ms The timeout in milliseconds.
 *
 * @return - ESP_OK on success
 * @return - ESP_FAIL if an error occurs
 */
esp_err_t nvs_set_can_timeout(uint32_t timeout_ms);

/**
 * @brief Loads the CAN receive timeout from nvs.
 *
 * @param[out] timeout_ms The timeout in milliseconds. Set to a default value if none was stored yet.
 *
 * @return - ESP_OK on success
 * @return - ESP_ERR_NVS_NOT_FOUND if the value has not yet been set
 * @return - ESP_ERR_INVALID_ARG if timeout_ms is NULL
 * @return - ESP_FAIL if an error occurs
 */
esp_err_t nvs_get_can_timeout(uint32_t *timeout_ms);

/**
 * @brief Updates the UDS response timeout in nvs.
 *
 * @param[in] timeout_ms The timeout in milliseconds.
 *
 * @return - ESP_OK on success
 * @return - ESP_FAIL if an error occurs
 */
esp_err_t nvs_set_uds_timeout(uint32_t timeout_ms);

/**
 * @brief Loads the UDS response timeout from nvs.
 *
 * @param[out] timeout_ms The timeout in milliseconds. Set to a default value if none was stored yet.
 *
 * @return - ESP_OK on success
 * @return - ESP_ERR_NVS_NOT_FOUND if the value has not yet been set
 * @return - ESP_ERR_INVALID_ARG if timeout_ms is NULL
 * @return - ESP_FAIL if an error occurs
 */
esp_err_t nvs_get_uds_timeout(uint32_t *timeout_ms);

/**
 * @brief Updates the deep sleep timeout in nvs.
 *
 * @param[in] timeout_s The timeout in seconds.
 *
 * @return - ESP_OK on success
 * @return - ESP_FAIL if an error occurs
 */
esp_err_t nvs_set_deepsleep_timeout(uint32_t timeout_s);

/**
 * @brief Loads the deep sleep timeout from nvs.
 *
 * @param[out] timeout_s The timeout in seconds. Set to a default value if none was stored yet.
 *
 * @return - ESP_OK on success
 * @return - ESP_ERR_NVS_NOT_FOUND if the value has not yet been set
 * @return - ESP_ERR_INVALID_ARG if timeout_s is NULL
 * @return - ESP_FAIL if an error occurs
 */
esp_err_t nvs_get_deepsleep_timeout(uint32_t *timeout_s);

/**
 * @brief Updates the interval between status term15 requests in nvs.
 *
 * @param[in] interval_ms The interval in milliseconds.
 *
 * @return - ESP_OK on success
 * @return - ESP_FAIL if an error occurs
 */
esp_err_t nvs_set_term15_request_interval(uint32_t interval_ms);

/**
 * @brief Loads the interval between status term15 requests from nvs.
 *
 * @param[out] interval_ms The interval in milliseconds. Set to a default value if none was stored yet.
 *
 * @return - ESP_OK on success
 * @return - ESP_ERR_NVS_NOT_FOUND if the value has not yet been set
 * @return - ESP_ERR_INVALID_ARG if interval_ms is NULL
 * @return - ESP_FAIL if an error occurs
 */
esp_err_t nvs_get_term15_request_interval(uint32_t *interval_ms);

