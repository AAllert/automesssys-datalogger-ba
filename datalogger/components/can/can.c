// implements
#include "can.h"

// system includes
#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

QueueHandle_t canReceiveQueue;

#define TAG "can"

static void can_rx_task(void *arg);
static esp_err_t start();

esp_err_t can_init(gpio_num_t can_tx_pin, gpio_num_t can_rx_pin)
{
    ESP_LOGI(TAG, "Initializing CAN driver");

    canReceiveQueue = xQueueCreate(10, sizeof(twai_message_t));
    if (canReceiveQueue == NULL) return ESP_ERR_NO_MEM;

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(can_tx_pin, can_rx_pin, TWAI_MODE_NORMAL);
    g_config.alerts_enabled = TWAI_ALERT_AND_LOG;
    static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    ESP_RETURN_ON_ERROR(twai_driver_install(&g_config, &t_config, &f_config), TAG, "Driver initilizing falied");
    ESP_RETURN_ON_ERROR(start(), TAG, "Starting the TWAI Controller Failed");

    return ESP_OK;
}

static esp_err_t start()
{
    ESP_LOGI(TAG, "Starting CAN driver");
    esp_err_t err = twai_start();
    ESP_RETURN_ON_ERROR(err, TAG, "Starting TWAI driver failed with err: %s", esp_err_to_name(err));
    ESP_LOGI(TAG, "Starting CAN receive task");
    // High priority to avoid dropped frames, because the RX Buffer is very small and must be drained promptly
    xTaskCreate(can_rx_task, "can_rx_task", 2048, NULL, 10, NULL);
    return ESP_OK;
}

// Continuously drains the TWAI driver's RX buffer and forwards frames to canReceiveQueue.
static void can_rx_task(void *arg)
{
    twai_message_t rx_msg;

    while (1) {
        esp_err_t err = twai_receive(&rx_msg, portMAX_DELAY);
        if (err != ESP_OK) {
            continue;
        }

        //ESP_LOG_BUFFER_HEX_LEVEL(TAG "-RX", rx_msg.data, rx_msg.data_length_code, ESP_LOG_INFO);
        //ESP_LOGI(TAG, "RX id=0x%lX extd=%d dlc=%d", rx_msg.identifier, rx_msg.extd, rx_msg.data_length_code);

        if (xQueueSend(canReceiveQueue, &rx_msg, 0) != pdTRUE) {
            //ESP_LOGW(TAG, "canReceiveQueue full, dropping frame with id 0x%lX", rx_msg.identifier);
        }
    }
}

esp_err_t send_can_message(twai_message_t *message)
{
    twai_status_info_t status_info;
    twai_get_status_info(&status_info);

    switch (status_info.state) {
    case TWAI_STATE_BUS_OFF: {
        // Too many failed transmissions, starts asynchronuous recovery
        esp_err_t err = twai_initiate_recovery();
        ESP_RETURN_ON_ERROR(err, TAG, "Failed to initiate recovery: %s", esp_err_to_name(err));
        return ESP_ERR_INVALID_STATE;
    }
    case TWAI_STATE_RECOVERING:
        return ESP_ERR_INVALID_STATE;
    case TWAI_STATE_STOPPED:
        // Recovery completed, driver must be restarted
        esp_err_t start_err = twai_start();
        ESP_RETURN_ON_ERROR(start_err, TAG, "Failed to restart TWAI driver: %s", esp_err_to_name(start_err));
        return ESP_ERR_INVALID_STATE;
    default:
        break;
    }

    esp_err_t err = twai_transmit(message, pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(err, TAG, "twai transmit failed with error: %s", esp_err_to_name(err));
    return ESP_OK;
}

esp_err_t receive_can_message(twai_message_t *message, uint32_t timeout_ms)
{
    if (xQueueReceive(canReceiveQueue, message, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t can_flush_receive_queue(void)
{
    if (canReceiveQueue == NULL) return ESP_ERR_INVALID_STATE;
    xQueueReset(canReceiveQueue);
    return ESP_OK;
}
