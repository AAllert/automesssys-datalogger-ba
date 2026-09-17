// implements
#include "can_backend.h"

// system includes
#include "driver/twai.h"
#include "string.h"

// project includes
#include "app_config.h"
#include "can.h"

esp_err_t can_backend_twai_init(void) {
    return can_init(PIN_NUM_CAN_TX, PIN_NUM_CAN_RX);
}

esp_err_t can_backend_twai_send(const can_frame_t *frame) {
    const uint32_t arbitration_id = frame->id;
    const uint8_t *data = frame->data;
    const uint8_t  size = frame->dlc;

    twai_message_t twai_message = {0};
    twai_message.identifier = arbitration_id;

    if (arbitration_id > 0x7FF) {
        twai_message.extd = 1;
    }

    twai_message.data_length_code = size;
    memcpy(twai_message.data, data, size);

    return send_can_message(&twai_message);
}

esp_err_t can_backend_twai_receive(can_frame_t *frame, uint32_t timeout_ms) {
    twai_message_t rx_msg;

    esp_err_t ret = receive_can_message(&rx_msg, timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }

    frame->id  = rx_msg.identifier;
    frame->dlc = rx_msg.data_length_code;
    memcpy(frame->data, rx_msg.data, rx_msg.data_length_code);

    return ESP_OK;
}

esp_err_t can_backend_twai_flush(void) {
    return can_flush_receive_queue();
}
