#pragma once

// system includes
#include <stdint.h>
#include "esp_err.h"

// project includes
#include "app_config.h"

typedef struct state_s state_t;

struct state_s {
    state_id_t state_id;
    const char *name;
    state_t* (*handler)(state_event_id_t event);
    state_t* (*on_enter)(void); // optional, called once when the state is entered, may be NULL; return NULL or the entered state itself to stay, 
};

/**
 * @brief Initilizes state machine
 */
esp_err_t state_machine_init(void);

/**
 * @brief Returns the current state of the state machine
 * 
 * @return The current State of the state machine
 */
state_t* state_machine_get_state(void);

/**
 * @brief Sets how long the system waits in wait_term_15 without a PIR trigger and disconnected wifi
 */
void set_sleep_timeout_s(uint32_t timeout_s);

/**
 * @brief Sets the interval between status term15 polling requests while waiting in wait_term_15.
 */
void set_term15_request_interval_ms(uint32_t interval_ms);
