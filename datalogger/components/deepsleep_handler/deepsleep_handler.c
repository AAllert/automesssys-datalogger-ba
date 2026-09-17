// implements
#include "deepsleep_handler.h"

// system includes
#include <stdbool.h>
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// project inlcudes
#include "app_config.h"
#include "event_loop.h"
#include "storage.h"
#include "status_indicator.h"

#define TAG "DeepSleepHandler"

static bool is_initilized = false;
static gpio_num_t pin_num_pir;
static gpio_num_t pin_num_mosfet;

esp_err_t deepsleep_handler_init(gpio_num_t mosfet_pin, gpio_num_t pir_pin) {
    pin_num_mosfet = mosfet_pin;
    pin_num_pir = pir_pin;
    gpio_hold_dis(pin_num_mosfet);
    gpio_config_t io_conf;

    // config for MOSFET Pin
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << pin_num_mosfet);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    is_initilized = true;
    return ESP_OK;
}

void go_to_deep_sleep() {
    deepsleep_power_down_peripherals();
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(1ULL << pin_num_pir, ESP_EXT1_WAKEUP_ANY_HIGH));

    ESP_LOGI(TAG, "Entering Deep Sleep");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_deep_sleep_start();
}

esp_err_t deepsleep_activate_peripherals() {
    if (!is_initilized) return ESP_ERR_INVALID_STATE;
    gpio_set_level(pin_num_mosfet, 1);
    return ESP_OK;
}

void deepsleep_power_down_peripherals() {
    sdcard_deinit();
    status_indicator_power_down();
    gpio_set_level(pin_num_mosfet, 0);
    gpio_hold_en(pin_num_mosfet);
}