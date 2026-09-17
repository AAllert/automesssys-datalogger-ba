#pragma once

// system includes
#include "esp_http_server.h"

// configuration
extern httpd_uri_t uri_set_time;
extern httpd_uri_t uri_get_time;
extern httpd_uri_t uri_get_active_config;
extern httpd_uri_t uri_set_active_config;
extern httpd_uri_t uri_get_can_timeout;
extern httpd_uri_t uri_set_can_timeout;
extern httpd_uri_t uri_get_uds_timeout;
extern httpd_uri_t uri_set_uds_timeout;
extern httpd_uri_t uri_get_deepsleep_timeout;
extern httpd_uri_t uri_set_deepsleep_timeout;
extern httpd_uri_t uri_get_term15_request_interval;
extern httpd_uri_t uri_set_term15_request_interval;

// datalogger
extern httpd_uri_t uri_start_logging;
extern httpd_uri_t uri_stop_logging;

// file
extern httpd_uri_t uri_list_configs;
extern httpd_uri_t uri_list_logs;
extern httpd_uri_t uri_list_csv_logs;
extern httpd_uri_t uri_upload_file;
extern httpd_uri_t uri_delete_file;

// ota
extern httpd_uri_t uri_ota_update;
