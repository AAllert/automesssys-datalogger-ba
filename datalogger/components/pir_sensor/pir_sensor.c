// implements
#include "pir_sensor.h"

// system inlcudes
#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// project includes
#include "app_config.h"
#include "event_loop.h"

#define TAG "PIR"
static bool is_initialized = false;
static QueueHandle_t pir_queue;

static gpio_num_t pin_num_pir;

static void IRAM_ATTR pir_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(pir_queue, &gpio_num, NULL);
}

static void pir_task(void *arg)
{
    uint32_t gpio_num;

    while (1) {
        if (xQueueReceive(pir_queue, &gpio_num, portMAX_DELAY)) {
            ESP_LOGI(TAG, "PIR triggered <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
            event_loop_publish_state_event(STATE_EVENT_PIR_TRIGGERED, NULL, 0);
        }
    }
}

// only for debugging, left in code because the PIR sensors are not reliable, useful for testing alternate sensors
static void gpio_poll_task(void *arg)
{
    static const char *POLL_TAG = "gpio";
    int last_level = -1;

    while (1) {
        int level = gpio_get_level(pin_num_pir);
        if (level != last_level) {
            ESP_LOGI(POLL_TAG, "GPIO%d: %s---------------------------------------------------------------------------------------", pin_num_pir, level ? "HIGH" : "LOW");
            last_level = level;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

esp_err_t pir_sensor_init(gpio_num_t pir_pin) {
    if (is_initialized) return ESP_OK;
    pin_num_pir = pir_pin;

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << pin_num_pir,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "gpio config failed");

    pir_queue = xQueueCreate(10, sizeof(uint32_t));
    ESP_RETURN_ON_ERROR(gpio_install_isr_service(0), TAG, "gpio isr service install failed");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(pin_num_pir, pir_isr_handler, (void *)pin_num_pir), TAG, "gpio isr handler add failed");
    xTaskCreate(pir_task, "pir_task", 2048, NULL, 10, NULL);

    xTaskCreate(gpio_poll_task, "gpio_poll", 2048, NULL, 3, NULL);

    is_initialized = true;
    return ESP_OK;
}