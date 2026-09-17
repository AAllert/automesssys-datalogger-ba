#pragma once

// system includes
#include "driver/gpio.h"
#include "esp_err.h"

// project includes
#include "app_config.h"

/**
 * @brief Initializes the status indicator.
 * 
 * @param[in] pin_num_red GPIO number connected to the red LED
 * @param[in] pin_num_yellow GPIO number connected to the yellow LED
 * @param[in] pin_num_green GPIO number connected to the green LED
 *
 * @return - ESP_OK on success.
 * @return - ESP_ERR_NO_MEM if memory could not be allocated.
 * @return - ESP_FAIL if an unexpected error occurs.
 */
esp_err_t status_indicator_init(gpio_num_t pin_num_red, gpio_num_t pin_num_yellow, gpio_num_t pin_num_green);

/**
 * @brief Set the current system state, which should be displayed by the LEDs.
 *
 * @param[in] state New status to indicate.
 */
void status_indicator_display_state(state_id_t state);

/**
 * @brief Powers down the status indicator leds.
 */
void status_indicator_power_down(void);

typedef enum {
    LED_DISPLAY_OFF,
    LED_DISPLAY_ON,
    LED_DISPLAY_BLINK
} led_display_t;

typedef struct {
    led_display_t red;
    led_display_t yellow;
    led_display_t green;
} led_pattern_t;

/**
 * @brief Returns which LED pattern is shown for the given status.
 *
 * @param[in] state Status which pattern should be returned.
 * 
 * @return The LED pattern, which holds the state for each of the three LEDs.
 */
led_pattern_t status_indicator_get_pattern(state_id_t state);
