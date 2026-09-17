/**
 * @file can_backend.c
 * Runtime dispatcher to forward all CAN communication eithor to the TWAI or JTAG implementation. 
 */

#include "can_backend.h"

esp_err_t can_backend_twai_init(void);
esp_err_t can_backend_twai_send(const can_frame_t *frame);
esp_err_t can_backend_twai_receive(can_frame_t *frame, uint32_t timeout_ms);
esp_err_t can_backend_twai_flush(void);

esp_err_t can_backend_usb_init(void);
esp_err_t can_backend_usb_send(const can_frame_t *frame);
esp_err_t can_backend_usb_receive(can_frame_t *frame, uint32_t timeout_ms);
esp_err_t can_backend_usb_flush(void);

static can_backend_type_t s_backend_type = CAN_BACKEND_CAN;

void can_backend_set_type(can_backend_type_t type)
{
    s_backend_type = type;
    const char *name = (type == CAN_BACKEND_UART) ? "USB/vECU": "TWAI/hardware";
}

esp_err_t can_backend_init(void)
{
    if (s_backend_type == CAN_BACKEND_UART) return can_backend_usb_init();
    return can_backend_twai_init();
}

esp_err_t can_backend_send(const can_frame_t *frame)
{
    if (s_backend_type == CAN_BACKEND_UART) return can_backend_usb_send(frame);
    return can_backend_twai_send(frame);
}

esp_err_t can_backend_receive(can_frame_t *frame, uint32_t timeout_ms)
{
    if (s_backend_type == CAN_BACKEND_UART) return can_backend_usb_receive(frame, timeout_ms);
    return can_backend_twai_receive(frame, timeout_ms);
}

esp_err_t can_backend_flush(void)
{
    if (s_backend_type == CAN_BACKEND_UART) return can_backend_usb_flush();
    return can_backend_twai_flush();
}
