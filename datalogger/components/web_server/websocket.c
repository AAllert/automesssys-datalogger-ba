// implements
#include "websocket.h"

// system includes
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"

// project includes
#include "event_loop.h"
#include "state_machine.h"
#include "status_indicator.h"
#include "datalogger_service.h"
#include "storage.h"

#define TAG "websocket"
#define MAX_WS_CLIENTS 4

static httpd_handle_t s_server = NULL;
static int s_clients[MAX_WS_CLIENTS];
static esp_timer_handle_t s_minute_timer = NULL;

/**
 * @brief Callback function for executing Code every Minute
 */
static void on_minute_tick(void *arg);

/**
 * @brief Schedules the next time update for the web interface.
 * @note Schedule the timer every time new to avoid drifting. 
 */
static void schedule_next_minute_tick(void);

static void add_client(int fd) {
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_clients[i] == 0) {
            s_clients[i] = fd;
            return;
        }
    }
    ESP_LOGW(TAG, "client list full, dropping fd %d", fd);
}

void websocket_on_close(httpd_handle_t hd, int sockfd) {
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_clients[i] == sockfd) s_clients[i] = 0;
    }
    close(sockfd);
}

static const char *led_str(led_display_t v) {
    switch (v) {
        case LED_DISPLAY_ON:    return "on";
        case LED_DISPLAY_BLINK: return "blink";
        default:                return "off";
    }
}

static char *build_status_json(void) {
    state_t *state = state_machine_get_state();
    led_pattern_t pattern = status_indicator_get_pattern(state->state_id);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "status");
    cJSON_AddStringToObject(root, "state", state->name);
    cJSON *leds = cJSON_AddObjectToObject(root, "leds");
    cJSON_AddStringToObject(leds, "red", led_str(pattern.red));
    cJSON_AddStringToObject(leds, "yellow", led_str(pattern.yellow));
    cJSON_AddStringToObject(leds, "green", led_str(pattern.green));

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static char *build_time_json(void) {
    time_t now;
    time(&now);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "time");
    cJSON_AddNumberToObject(root, "timestamp", (double)now);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static char *build_cycles_json() {
    uint32_t cycles = get_num_cycles();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "cycles");
    cJSON_AddNumberToObject(root, "cycles", cycles);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static char *build_timeouts_json() {
    uint32_t timeouts = get_num_timeouts();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "timeouts");
    cJSON_AddNumberToObject(root, "timeouts", timeouts);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static char *build_storage_usage_json() {
    uint64_t free;
    uint64_t used;
    uint64_t total;
    get_sdcard_usage(&free, &used, &total);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "storage");
    cJSON_AddNumberToObject(root, "free", free);
    cJSON_AddNumberToObject(root, "used", used);
    cJSON_AddNumberToObject(root, "total", total);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static void send_frame_to(int fd, const char *json) {
    httpd_ws_frame_t pkt = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)json,
        .len = strlen(json)
    };
    esp_err_t err = httpd_ws_send_frame_async(s_server, fd, &pkt);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "send to fd %d failed: %s", fd, esp_err_to_name(err));
    }
}

static void broadcast_work(void *arg) {
    char *json = (char *)arg;
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_clients[i] != 0) send_frame_to(s_clients[i], json);
    }
    free(json);
}

static void broadcast_json(char *json) {
    if (s_server == NULL || httpd_queue_work(s_server, broadcast_work, json) != ESP_OK) {
        ESP_LOGW(TAG, "failed to queue broadcast");
        free(json);
    }
}

static void on_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data) {
    broadcast_json(build_status_json());
}

static void on_system_event(void *arg, esp_event_base_t base, int32_t event_id, void *data) {
    switch (event_id) {
    case SYSTEM_EVENT_NEW_CYCLE:
        broadcast_json(build_cycles_json());
        break;
    case SYSTEM_EVENT_TIMEOUT_REACHED:
        broadcast_json(build_timeouts_json());
        break;
    default:
        break;
    }
}

static void schedule_next_minute_tick(void) {
    time_t now;
    time(&now);
    int64_t seconds_to_next_minute = 60 - (now % 60);

    if (s_minute_timer == NULL) {
        const esp_timer_create_args_t args = {
            .callback = &on_minute_tick,
            .name = "ws_minute_tick"
        };
        esp_timer_create(&args, &s_minute_timer);
    }
    esp_timer_start_once(s_minute_timer, (uint64_t)seconds_to_next_minute * 1000000ULL);
}

static void on_minute_tick(void *arg) {
    broadcast_json(build_time_json());
    broadcast_json(build_status_json());
    schedule_next_minute_tick();
}

static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        // handshake just completed, connection is open
        int fd = httpd_req_to_sockfd(req);
        add_client(fd);

        // initial snapshot for the freshly connected client
        char *status_json = build_status_json();
        send_frame_to(fd, status_json);
        free(status_json);

        char *time_json = build_time_json();
        send_frame_to(fd, time_json);
        free(time_json);

        char *cycle_json = build_cycles_json(get_num_cycles());
        send_frame_to(fd, cycle_json);
        free(cycle_json);

        char *timeout_json = build_timeouts_json(get_num_timeouts());
        send_frame_to(fd, timeout_json);
        free(timeout_json);


        char * storage_json = build_storage_usage_json();
        send_frame_to(fd, storage_json);
        free(storage_json);

        return ESP_OK;
    }

    httpd_ws_frame_t pkt = { .type = HTTPD_WS_TYPE_TEXT };
    esp_err_t ret = httpd_ws_recv_frame(req, &pkt, 0);
    if (ret != ESP_OK) return ret;

    if (pkt.type == HTTPD_WS_TYPE_CLOSE) {
        int fd = httpd_req_to_sockfd(req);
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (s_clients[i] == fd) s_clients[i] = 0;
        }
    }
    return ESP_OK;
}

void websocket_init(httpd_handle_t server) {
    s_server = server;
    esp_event_handler_register_with(app_event_loop, STATE_CHANGED_EVENT, ESP_EVENT_ANY_ID, on_state_changed, NULL);
    esp_event_handler_register_with(app_event_loop, SYSTEM_EVENT, ESP_EVENT_ANY_ID, on_system_event, NULL);
    schedule_next_minute_tick();
}

httpd_uri_t uri_websocket = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = ws_handler,
    .user_ctx = NULL,
    .is_websocket = true
};
