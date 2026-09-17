// implements
#include "log_manager.h"

// system includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"

// project includes 
#include "storage.h"

#define LOG_PATH "/sdcard/logs/"
#define TAG "log_manager"

static esp_err_t log_entry_to_string(const LogEntry *log_entry, char **string);

static char current_path[128] = {0};
static FILE *current_log = NULL;

esp_err_t start_new_log_file(const char *filename, const char *car_name) {
    if (filename == NULL && car_name == NULL) return ESP_ERR_INVALID_ARG;
    if (mkdir(LOG_PATH, 0777) != 0 && errno != EEXIST) {
        ESP_LOGE(TAG, "directory for logs could not be created");
        return ESP_FAIL;
    }
    if (filename == NULL) {
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        snprintf(current_path, sizeof(current_path), LOG_PATH "raw_%s_%04d-%02d-%02d_%02d-%02d-%02d.log",
                car_name, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    } else {
        snprintf(current_path, sizeof(current_path), "%s%s", LOG_PATH, filename);
    }

    if (current_log != NULL) {
        fclose(current_log);
    }
    current_log = fopen(current_path, "a");
    if (current_log == NULL) {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Logfile opened");
    return ESP_OK;
}

void close_log_file() {
    if (current_log != NULL) {
        fclose(current_log);
        current_log = NULL;
    }
}

void flush_log() {
    if (current_log == NULL) {
        return;
    }
    fflush(current_log);
    fsync(fileno(current_log));
}

esp_err_t append_to_log(LogEntry *log_entry) {
    if (log_entry == NULL) return ESP_ERR_INVALID_ARG;
    if (current_log == NULL) return ESP_ERR_INVALID_STATE;

    char *log_string = NULL;
    esp_err_t result = log_entry_to_string(log_entry, &log_string);
    if (result != ESP_OK) {
        return result;
    }
    fprintf(current_log, "%s", log_string);
    free(log_string);
    ESP_LOGI(TAG, "appended to log successfully");
    return ESP_OK;
}

esp_err_t delete_log(const char *filename) {
    if (filename == NULL) return ESP_ERR_INVALID_ARG;
    char path[256];
    snprintf(path, sizeof(path), "%s%s", LOG_PATH, filename);
    return storage_delete_file(path);
}

bool list_collected_logs(FileInfo *files, size_t offset, size_t max_files, size_t *num_files) {
    return storage_list_files(LOG_PATH, ".log", files, offset, max_files, num_files);
}

static esp_err_t log_entry_to_string(const LogEntry *log_entry, char **string) {
    if (log_entry == NULL || string == NULL) return ESP_ERR_INVALID_ARG;
    *string = NULL;

    // calc necessary memory
    size_t size = 32 + strlen(log_entry->name) + 9; // <timestamp> - <Name> - b''\n
    if (log_entry->data_length > 0) {
        size += log_entry->data_length * 3; // max. datalength * ff
    }
    char *buffer = malloc(size);
    if (buffer == NULL) return ESP_ERR_NO_MEM;

    int64_t seconds = log_entry->timestamp_us / 1000000;
    int64_t micros  = log_entry->timestamp_us % 1000000;

    size_t pos = snprintf(buffer, size, "%lld.%06lld - %s - b'", (long long)seconds, (long long)micros, log_entry->name);
    for (uint16_t i = 0; i < log_entry->data_length; i++) {
        pos += snprintf(buffer + pos, size - pos, "%02x%s", log_entry->data[i], (i < log_entry->data_length - 1) ? "," : "");
    }
    snprintf(buffer + pos, size - pos, "'\n");

    *string = buffer;
    return ESP_OK;
}
