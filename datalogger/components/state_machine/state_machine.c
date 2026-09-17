// implements
#include "state_machine.h"

// system includes
#include "esp_log.h"

// project includes
#include "event_loop.h"

#define TAG "StateMachine"

extern state_t state_initilizing;

static state_t *current_state = &state_initilizing;

static void transition_to(state_t *next_state);

static void run_on_enter(void) {
    if (!current_state->on_enter) return;
    state_t *next = current_state->on_enter();
    if (next != NULL && next != current_state) transition_to(next);
}

static void transition_to(state_t *next_state) {
    ESP_LOGI(TAG, "%s -> %s", current_state->name, next_state->name);
    current_state = next_state;
    event_loop_publish_state_changed(next_state->state_id);
    run_on_enter();
}

static void on_state_event(void *arg, esp_event_base_t base, int32_t event_id, void *data) {
    state_t *next = current_state->handler((state_event_id_t)event_id);
    if (next != current_state) transition_to(next);
}

esp_err_t state_machine_init(void) {
    run_on_enter();
    return esp_event_handler_register_with(app_event_loop, STATE_EVENT, ESP_EVENT_ANY_ID, on_state_event, NULL);
}

state_t* state_machine_get_state(void) {
    return current_state;
}
