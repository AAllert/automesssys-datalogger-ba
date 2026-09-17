//system includes
#include "esp_log.h"

//project inlcudes
#include "state_machine.h"
#include "event_loop.h"
#include "can_backend.h"

void app_main(void)
{
    ESP_LOGI("main", "Called app_main, App started");

    // Init event bus system and state machine first, so that other components can communicate
    // Other initilization will be done in the state machine in the init state enter handler which automatically called first
    event_bus_init();
    can_backend_set_type(CAN_BACKEND_CAN);
    state_machine_init();
}
