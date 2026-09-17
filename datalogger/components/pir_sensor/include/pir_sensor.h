#pragma once

// system includes
#include "esp_err.h"
#include "driver/gpio.h"

/**
 * @brief Initilizes the PIR Sensor. 
 * 
 * This includes the GPIO Pin config and the event loop subscriber.
 * 
 * @param[in] pir_pin The GPIO pin number of the pin connected to the signal pin of the PIR sensor.
 * 
 * @return - ESP_OK on success
 */
esp_err_t pir_sensor_init(gpio_num_t pir_pin);