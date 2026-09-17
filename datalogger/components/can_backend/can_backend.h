#pragma once

// system includes
#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Specifies the available CAN- backend implementations.
 * 
 * CAN_BACKEND_CAN, CAN_BACKEND_UART
 */
typedef enum
{
    CAN_BACKEND_CAN,    // Uses the hardware TWAI implementation
    CAN_BACKEND_UART,   // Uses the USB-serial mock implementation (HIL testing with vECU)
} can_backend_type_t;

/**
 * @brief generic CAN frame representation
 * 
 * Fields: uint32_t id, uint8_t dlc, uint8_t data[8]
 */
typedef struct {
    uint32_t id;        // CAN ID (11-bit or extended 29-bit)
    uint8_t dlc;        // Data Length Code, specifies the length of the payload (0 to 8)
    uint8_t data[8];    // Payload bytes of the CAN frame
} can_frame_t;

/**
 * @brief Selects which CAN backend implementation to use.
 *
 * Must be called before can_backend_init(). Defaults to CAN_BACKEND_CAN.
 *
 * @param[in] type CAN_BACKEND_CAN for hardware TWAI, CAN_BACKEND_UART for USB/vECU
 */
void can_backend_set_type(can_backend_type_t type);

/**
 * @brief Initilizes the CAN-Controller.
 */
esp_err_t can_backend_init(void);

/** 
 * @brief Sends a CAN frame via the CAN bus to the car.
 * 
 * @param[in] frame Can Frame to be send
 */
esp_err_t can_backend_send(const can_frame_t *frame);

/**
 * @brief Receives a CAN frame from the car. 
 * 
 * @note This method blocks!
 * 
 * @param[out] frame CAN frame which will be received
 * @param[in] timeout_ms Timeout, how long to wait for a response, in milliseconds
 */
esp_err_t can_backend_receive(can_frame_t *frame, uint32_t timeout_ms);

/**
 * @brief Discards any CAN frames that have already been received but not yet consumed by can_backend_receive().
 * 
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_STATE if the can_backend was not initilized yet.
 */
esp_err_t can_backend_flush(void);