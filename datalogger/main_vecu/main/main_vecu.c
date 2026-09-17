// system includes
#include "esp_log.h"

// project includes
#include "can_backend.h"
#include "state_machine.h"
#include "event_loop.h"

// ── Entry point ────────────────────────────────────────────────────────────

void app_main(void)
{
    ESP_LOGI("main", "Called app_main for the vecu, App started");

    event_bus_init();
    can_backend_set_type(CAN_BACKEND_UART);
    state_machine_init();
}

