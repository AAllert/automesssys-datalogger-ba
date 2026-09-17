#pragma once

#include <stddef.h>
#include "esp_err.h"

#define USB_BUFFER_SIZE 1024

/**
 * @brief Initilizes the usb serial / jtag interface.
 */
esp_err_t usb_serial_init(void);

/**
 * @brief Writes a line on the usb_serial/jtag interface. 
 * 
 * @param line A cahr array to the line which should be sent. 
 */
esp_err_t usb_serial_write_line(const char *line);

/**
 * @brief Reads a line from the usb_serial/jtag interface. 
 * 
 * This functions returns, when a full line was read or the timeout is reached. 
 * @note A full line is indicated by '\n'.
 * 
 * @param buf A pointer array, in which the reault shoul be stored. 
 * @param buf_size The size of the buffer. This is the maximum length the line can have.
 * @param timeout The maximum time to wait for a full line. 
 */
esp_err_t usb_serial_read_line(char *buf, size_t buf_size, int timeout_ms);

/**
 * @brief Discards any bytes currently buffered from the usb_serial/jtag interface.
 */
void usb_serial_flush_input(void);
