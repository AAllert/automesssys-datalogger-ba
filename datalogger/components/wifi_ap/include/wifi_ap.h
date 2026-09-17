#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Initilizes the wifi access point. 
 */
esp_err_t wifi_ap_init(void);

/**
 * @brief Opens a wifi access point.
 * 
 * @return - ESP_OK on success
 */
esp_err_t wifi_ap_start(void);

/**
 * @brief Closes the opened wifi access point.
 * 
 * @return - ESP_OK on success
 */
esp_err_t wifi_ap_close(void);

/**
 * @brief Returns whether at least one station is currently connected to the AP.
 */
bool wifi_ap_is_client_connected(void);