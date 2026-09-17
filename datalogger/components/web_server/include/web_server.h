#pragma once

// system includes
#include "esp_err.h"

/**
 * @brief Starts the webserver for the web configuration interface of the datalogger. 
 * 
 * @return - ESP_OK on success
 * @return - ESP_ERROR_INVALID_STATE if the webserver is already running
 * @return - ESP_ERR_HTTPD_ALLOC_MEM Failed to allocate memory for instance 
 * @return - ESP_ERR_HTTPD_TASK Failed to launch server task
 */
esp_err_t web_server_start(void);

/**
 * @brief Stops the webserver.
 *
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_STATE if the webserver is not running
 */
esp_err_t web_server_stop(void);