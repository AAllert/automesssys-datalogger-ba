#pragma once

// system includes
#include "esp_err.h"
#include "driver/gpio.h"

/**
 * @brief initilizes the deepsleep handler.
 * 
 * @param[in] mosfet_pin The GPIO pin number of the pin connected to the MOSFET.
 * @param[in] pir_pin The GPIO pin number of the pin onnected to the PIR sensor.
 * 
 * @return - ESP_OK on success
 * @return - ESP_ERR_NO_MEM if necessary memory could not allocated
 * @return - ESP_FAIL if an unexpected error occurs
 */
esp_err_t deepsleep_handler_init(gpio_num_t mosfet_pin, gpio_num_t pir_pin);

/**
 * @brief Sets the ESP-C6 to deepsleep mode. 
 * 
 * @note if an error accurs, the app will crash. 
 */
void go_to_deep_sleep();

/**
 * @brief Activates power for peripherals by activating the MOSFET.
 * 
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_STATE if the deepsleep handler was not already initilized.
 */
esp_err_t deepsleep_activate_peripherals();

/**
 * @brief Deactivates the peripherals by closing the MOSFET. 
 */
void deepsleep_power_down_peripherals();