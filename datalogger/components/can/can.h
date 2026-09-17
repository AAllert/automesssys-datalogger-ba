#pragma once

// system includes
#include "driver/twai.h"
#include "esp_err.h"

/**
 * @brief Initilizes the TWAI driver.
 * 
 * @param[in] can_tx_pin The GPIO pin number for the CAN Tranceiver pin.
 * @param[in] can_rx_pin The GPIO pin number for CAN Receiver pin.
 * 
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_ARG: Arguments are invalid 
 * @return - ESP_ERR_NO_MEM: Insufficient memory
 * @return - ESP_ERR_INVALID_STATE: Driver is already installed / new installing failed
 */
esp_err_t can_init(gpio_num_t can_tx_pin, gpio_num_t can_rx_pin);

/**
 * @brief Sends a CAN message directly to the TWAI driver.
 *
 * @param message The message which should be sent to the ECU.
 *
 * @return - ESP_OK on success
 */
esp_err_t send_can_message(twai_message_t *message);

/**
 * @brief Reads a CAN message from the receive queue.
 *
 * @param[out] message the received can message
 * @param[in] timeout_ms how long to wait for a message, in milliseconds
 *
 * @return - ESP_OK on success
 * @return - ESP_ERR_TIMEOUT: Timed out waiting for message
 */
esp_err_t receive_can_message(twai_message_t *message, uint32_t timeout_ms);

/**
 * @brief Discards any CAN messages already buffered in canReceiveQueue.
 *
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_STATE if the CAN driver has not been initialized
 */
esp_err_t can_flush_receive_queue(void);