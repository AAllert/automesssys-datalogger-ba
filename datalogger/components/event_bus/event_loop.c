// implements
#include "event_loop.h"

// system includes
#include "esp_event.h"
#include "esp_err.h"

ESP_EVENT_DEFINE_BASE(STATE_EVENT);
ESP_EVENT_DEFINE_BASE(STATE_CHANGED_EVENT);
ESP_EVENT_DEFINE_BASE(SYSTEM_EVENT);

esp_event_loop_handle_t app_event_loop = NULL;

esp_err_t event_bus_init(void)
{
    esp_event_loop_args_t loop_args = {
        .queue_size      = 32,
        .task_name       = "app_evt",
        .task_priority   = 5,
        .task_stack_size = 4096,
        .task_core_id    = tskNO_AFFINITY
    };
    return esp_event_loop_create(&loop_args, &app_event_loop);
}

esp_err_t event_loop_publish_state_event(state_event_id_t event_id, const void *event_data, size_t event_data_size) {
    if (app_event_loop == NULL) return ESP_ERR_INVALID_STATE;
    return esp_event_post_to(app_event_loop, STATE_EVENT, event_id, event_data, event_data_size, portMAX_DELAY);
}

esp_err_t event_loop_publish_state_changed(state_id_t new_state) {
    if (app_event_loop == NULL) return ESP_ERR_INVALID_STATE;
    return esp_event_post_to(app_event_loop, STATE_CHANGED_EVENT, new_state, NULL, 0, portMAX_DELAY);
}

esp_err_t event_loop_publish_system_event(system_event_id_t event_id, const void *event_data, size_t event_data_size) {
    if (app_event_loop == NULL) return ESP_ERR_INVALID_STATE;
    return esp_event_post_to(app_event_loop, SYSTEM_EVENT, event_id, event_data, event_data_size, 0);
}
