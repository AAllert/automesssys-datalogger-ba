#pragma once

// system includes
#include "esp_http_server.h"

extern httpd_uri_t uri_websocket;

/**
 * @brief Initilizes the websocket.
 *
 * @param[in] server Handle of the already-started httpd server.
 */
void websocket_init(httpd_handle_t server);

/**
 * @brief Closes the websocket.
 */
void websocket_on_close(httpd_handle_t hd, int sockfd);
