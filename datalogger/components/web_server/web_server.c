// implements
#include "web_server.h"

// system includes
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "mdns.h"
#include "esp_check.h"

// project includes
#include "rest_endpoints.h"
#include "websocket.h"
#include "storage.h"

#define TAG "web_server"

static httpd_handle_t s_server = NULL;

static esp_err_t mdns_start(void)
{
    ESP_RETURN_ON_ERROR(mdns_init(), TAG, "mdns_init failed");
    ESP_RETURN_ON_ERROR(mdns_hostname_set("automessy"), TAG, "mdns_set_hostname failed");
    ESP_RETURN_ON_ERROR(mdns_instance_name_set("AutoMessy ESP32"), TAG, "mdns_set_instance_name failes");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "mDNS: automessy.local");
    return ESP_OK;
}

static void mdns_stop(void)
{
    mdns_free();
}

/**
 * @brief Register endpoints with centralized error handling (printing)
 */
static void register_uri(const httpd_uri_t *uri)
{
    esp_err_t err = httpd_register_uri_handler(s_server, uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register handler for %s: %s", uri->uri, esp_err_to_name(err));
    }
}

static const char *content_type(const char *path)
{
    if (strstr(path, ".html")) return "text/html";
    if (strstr(path, ".css"))  return "text/css";
    if (strstr(path, ".js"))   return "application/javascript";
    if (strstr(path, ".ico"))  return "image/x-icon";
    if (strstr(path, ".svg"))  return "image/svg+xml";
    if (strstr(path, ".png"))  return "image/png";
    if (strstr(path, ".jpg") || strstr(path, ".jpeg")) return "image/jpeg";
    if (strstr(path, ".json")) return "application/json";
    return "text/plain";
}

static esp_err_t static_handler(httpd_req_t *req)
{
    const char *uri = req->uri;

    if (strcmp(uri, "/") == 0) {
        uri = "/index.html";
    }

    // /configs/* and /logs/* are resolved to the SD card, everything else is served from the SPIFFS-based web UI
    char filepath[520];
    bool is_data_file = storage_resolve_data_path(uri, filepath, sizeof(filepath));
    if (!is_data_file) {
        snprintf(filepath, sizeof(filepath), "/spiffs%s", uri);
    }

    FILE *f = fopen(filepath, "r");
    if (!f) {
        ESP_LOGW(TAG, "Nicht gefunden: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, is_data_file ? "application/octet-stream" : content_type(filepath));

    char chunk[512];
    size_t n;
    do {
        n = fread(chunk, 1, sizeof(chunk), f);
        if (n > 0 && httpd_resp_send_chunk(req, chunk, n) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
    } while (n == sizeof(chunk));

    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t web_server_start(void) {
    if (s_server != NULL) return ESP_ERR_INVALID_STATE;
    httpd_config_t cfg  = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.close_fn = websocket_on_close;
    cfg.max_uri_handlers = 24;
    cfg.max_open_sockets = 10;
    cfg.lru_purge_enable = true;

    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &cfg), TAG, "Starting the webserver failed");

    register_uri(&uri_set_time);
    register_uri(&uri_get_time);
    register_uri(&uri_start_logging);
    register_uri(&uri_stop_logging);
    register_uri(&uri_set_active_config);
    register_uri(&uri_get_active_config);
    register_uri(&uri_get_can_timeout);
    register_uri(&uri_set_can_timeout);
    register_uri(&uri_get_uds_timeout);
    register_uri(&uri_set_uds_timeout);
    register_uri(&uri_get_deepsleep_timeout);
    register_uri(&uri_set_deepsleep_timeout);
    register_uri(&uri_get_term15_request_interval);
    register_uri(&uri_set_term15_request_interval);
    register_uri(&uri_list_configs);
    register_uri(&uri_list_logs);
    register_uri(&uri_list_csv_logs);
    register_uri(&uri_ota_update);

    // register file endpoints with wildcard at last
    register_uri(&uri_upload_file);
    register_uri(&uri_delete_file);
    register_uri(&uri_websocket);
    websocket_init(s_server);
    static const httpd_uri_t files = {
        .uri     = "/*",
        .method  = HTTP_GET,
        .handler = static_handler,
    };
    register_uri(&files);

    mdns_start();
    return ESP_OK;
}

esp_err_t web_server_stop(void) {
    if (s_server == NULL) return ESP_ERR_INVALID_STATE;
    ESP_RETURN_ON_ERROR(httpd_stop(s_server), TAG, "Stopping webserver failed");
    s_server = NULL;
    mdns_stop();
    return ESP_OK;
}