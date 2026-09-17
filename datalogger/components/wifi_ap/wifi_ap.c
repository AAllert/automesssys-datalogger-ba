// implements
#include "wifi_ap.h"

// system includes
#include <string.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_check.h"

// project includes
#include "event_loop.h"

#define AP_SSID     "AutoMessy"
#define AP_PASS     "automessy123"
#define AP_CHANNEL  1
#define AP_MAX_CONN 4

static const char *TAG = "wifi_ap";

static int connected_stations = 0;
static bool is_initilized = false;
static bool is_running = false;
static esp_netif_t *s_ap_netif = NULL;
static esp_event_handler_instance_t s_wifi_event = NULL;

esp_err_t wifi_ap_init() {
    if (is_initilized) return ESP_OK;
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop creation failed");
    is_initilized = true;
    return ESP_OK;
}

static void event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *e = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "Client verbunden: %02x:%02x:%02x:%02x:%02x:%02x", e->mac[0], e->mac[1], e->mac[2], e->mac[3], e->mac[4], e->mac[5]);
        connected_stations++;
        if (connected_stations == 1) {
            esp_event_post_to(app_event_loop, STATE_EVENT, STATE_EVENT_WIFI_CONNECTED, NULL, 0, portMAX_DELAY);
        }
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *e = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "Client getrennt: %02x:%02x:%02x:%02x:%02x:%02x", e->mac[0], e->mac[1], e->mac[2], e->mac[3], e->mac[4], e->mac[5]);
        if (connected_stations > 0) {
            connected_stations--;
        }
        if (connected_stations == 0) {
            esp_event_post_to(app_event_loop, STATE_EVENT, STATE_EVENT_WIFI_DISCONNECTED, NULL, 0, portMAX_DELAY);
        }
    }
}

esp_err_t wifi_ap_start(void)
{
    if (!is_initilized || is_running) return ESP_ERR_INVALID_STATE;

    s_ap_netif = esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "initilizing wifi failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &s_wifi_event), TAG, "registering event handler failed");

    wifi_config_t wifi_config = {
        .ap = {
            .ssid        = AP_SSID,
            .ssid_len    = strlen(AP_SSID),
            .channel     = AP_CHANNEL,
            .password    = AP_PASS,
            .max_connection = AP_MAX_CONN,
            .authmode    = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "Setting wifi mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &wifi_config), TAG, "setting config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "starting wifi failed");

    ESP_LOGI(TAG, "WIFI AP started - SSID: %s  IP: 192.168.4.1", AP_SSID);
    is_running = true;
    return ESP_OK;
}

esp_err_t wifi_ap_close(void)
{
    if (!is_initilized || !is_running) return ESP_ERR_INVALID_STATE;
    ESP_RETURN_ON_ERROR(esp_wifi_stop(), TAG, "stop wifi while closing access point failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_event), TAG, "Unregistering of wifi ap event handler failed");
    ESP_RETURN_ON_ERROR(esp_wifi_deinit(), TAG, "Deinit of wifi while closing acces point failed");
    if (s_ap_netif) {
        esp_netif_destroy(s_ap_netif);
        s_ap_netif = NULL;
    }
    connected_stations = 0;
    is_running = false;
    return ESP_OK;
}

bool wifi_ap_is_client_connected(void)
{
    return connected_stations > 0;
}
