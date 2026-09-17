#pragma once

// system includes
#include "esp_event.h"
#include "esp_err.h"

// project includes
#include "app_config.h"

ESP_EVENT_DECLARE_BASE(STATE_EVENT);
ESP_EVENT_DECLARE_BASE(STATE_CHANGED_EVENT);
ESP_EVENT_DECLARE_BASE(SYSTEM_EVENT);

extern esp_event_loop_handle_t app_event_loop;

/**
 * @brief Initilizes the event loop used by the state machine. 
 * 
 * @return ESP_OK on success
 * @return ESP_FAIL if an error occurs
 */
esp_err_t event_bus_init(void);

/**
 * @brief Publishes an event for the state machine
 * 
 * @param[in] event_id The ID of the State Event
 * @param[in] event_data Optional data associated with the event
 * @param[in] event_data_size The size of the optional data
 */
esp_err_t event_loop_publish_state_event(state_event_id_t event_id, const void *event_data, size_t event_data_size);

/**
 * @brief Publishes an event from the state machine, when a state changed. 
 * 
 * @param[in] new_state The new state the state machine transitioned to.
 * 
 * @note This function should only be used by the state machine.
 */
esp_err_t event_loop_publish_state_changed(state_id_t new_state);

/**
 * @brief publishes a general system event, like telemetry change. 
 * 
 * This event base is used for all communication between the componenents among themselves.
 * 
 * @param[in] event_id The ID of the system event
 * @param[in] event_data Optional data associated with the event
 * @param[in] event_data_size The size of the optional data
 */
esp_err_t event_loop_publish_system_event(system_event_id_t event_id, const void *event_data, size_t event_data_size);
