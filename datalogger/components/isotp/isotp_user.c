/** @file isotp_user.c
 * Implements the hook functions of the ISO-TP (ISO 15765-2) Support Library (https://github.com/lishen2/isotp-c/).
 */

#include "isotp_user.h"
#include <stdint.h>
#include "isotp_defines.h"
#include "esp_timer.h"
#include <string.h>
#include "can_backend.h"

void isotp_user_debug(const char *message, ...)
{
    //ESP_LOGI(TAG, "%s", message);
}

//send data via CAN
int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size)
{
    //ESP_LOGI(TAG, "sending can message with id 0x%02lX", arbitration_id);
    //ESP_LOG_BUFFER_HEXDUMP(TAG, data, size, ESP_LOG_INFO);
    //ESP_LOGI(TAG, "size: %u", size);

    if (size > 8 || data == NULL) {
        return ISOTP_RET_ERROR;
    }

    can_frame_t frame = {
        .id  = arbitration_id,
        .dlc = size
    };
    memcpy(frame.data, data, size);

    esp_err_t err = can_backend_send(&frame);
    return (err == ESP_OK) ? ISOTP_RET_OK : ISOTP_RET_ERROR;
}   

/* user implemented, get millisecond */
uint32_t isotp_user_get_ms(void)
{
    int64_t time_us = esp_timer_get_time();
    // Convert to milliseconds and cast to uint32_t
    uint32_t time_ms = (uint32_t)(time_us / 1000);
    return time_ms;
}
