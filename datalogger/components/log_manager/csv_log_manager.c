// implements
#include "csv_log_manager.h"

// system includes
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"

// project includes
#include "uds_decoder.h"
#include "interpolator.h"

#define LOG_PATH "/sdcard/logs/"
#define TAG "log_manager"
#define MAX_LOG_SIGNALS 254

static char current_path[128] = {0};
static FILE *current_log = NULL;

typedef struct {
    datatype_t type;
    union {
        uint32_t u;
        int32_t i;
        float f;
    } value;
}
LogValue;

esp_err_t start_new_csv_log_file(const char *filename, UdsConfig config) {
    if (mkdir(LOG_PATH, 0777) != 0 && errno != EEXIST) {
        ESP_LOGE(TAG, "directory for logs could not be created");
        return ESP_FAIL;
    }
    if (filename == NULL) {
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        snprintf(current_path, sizeof(current_path), LOG_PATH "log_%s_%04d-%02d-%02d_%02d-%02d-%02d.csv",
                config.car_name, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    } else {
        snprintf(current_path, sizeof(current_path), "%s%s", LOG_PATH, filename);
    }

    if (current_log != NULL) fclose(current_log);
    current_log = fopen(current_path, "a");
    interpolator_start_session(config);

    // Write labels from the config to the csv header
    fprintf(current_log, "timestamp");
    for (int i=0;i<config.num_rows;i++) {
        fprintf(current_log, ",%s", config.rows[i].name);
    }
    fputc('\n', current_log);
    return ESP_OK;
}


void close_csv_log_file() {
    if (current_log != NULL) {
        fclose(current_log);
        current_log = NULL;
    }
}

void flush_csv_log() {
    if (current_log == NULL) return;
    fflush(current_log);
    fsync(fileno(current_log));
}

static void write_log_line(uint64_t timestamp_us, LogValue *values, size_t count) {
    int64_t seconds = timestamp_us / 1000000;
    int64_t micros  = timestamp_us % 1000000;
    fprintf(current_log, "%lld.%06lld", seconds, micros);
    for (size_t i=0;i<count;i++) {
        switch (values[i].type) {
        case UINT:
            fprintf(current_log, ",%" PRIu32, values[i].value.u);
            break;
        case INT:
            fprintf(current_log, ",%" PRId32, values[i].value.i);
            break;
        case FLOAT32:
            fprintf(current_log, ",%.6f", values[i].value.f);
            break;
        default:
            break;
        }
    }
    fputc('\n', current_log);
}

esp_err_t append_csv_log_row(UdsResponse *responses, UdsConfig config) {
    if (responses == NULL) return ESP_ERR_INVALID_ARG;
    if (current_log == NULL) return ESP_ERR_INVALID_STATE;
    if (config.num_rows == 0 || config.num_rows > MAX_LOG_SIGNALS) return ESP_ERR_INVALID_ARG;
    LogValue values[config.num_rows];
    memset(values, 0, sizeof(values));
    bool all_interpolated = true;

    for (int i=0;i<config.num_rows;i++) {
        uint16_t expected_did = ((uint16_t)config.rows[i].requestParameters[0] << 8) | config.rows[i].requestParameters[1];
        const uint8_t *payload;
        size_t payload_size;
        if (uds_extract_rdbi_payload(responses[i].data, responses[i].size, expected_did, &payload, &payload_size) != ESP_OK) {
            all_interpolated = false;
            continue;
        }

        uint32_t raw_value = extract_bits(payload, config.rows[i].startBit, config.rows[i].length, config.rows[i].byte_order);
        double decoded = (config.rows[i].strType == FLOAT32)
            ? decode_raw_float32_value(raw_value)
            : decode_raw_value(raw_value, config.rows[i].scale, config.rows[i].offset);
        double interpolated;
        if(interpolate(responses[i].name, responses[i].timestamp_us, decoded, &interpolated)) {
            double result;
            if (config.rows[i].should_interpolate){
                result = interpolated;
            } else {
                result = decoded;
            }
            switch (config.rows[i].strType) {
            case UINT:
                values[i] = (LogValue){.type = UINT, .value.u = (uint32_t) result};
                break;
            case INT:
                values[i] = (LogValue){.type = INT, .value.i = (int32_t) result};
                break;
            case FLOAT32:
                values[i] = (LogValue){.type = FLOAT32, .value.f = (float) result};
                break;
            default:
                break;
            }
        } else {
            all_interpolated = false;
        }
    }
    if (all_interpolated) {
        write_log_line(interpolator_get_target_timestamp(), values, config.num_rows);
    }
    interpolator_set_target_timestamp(responses[config.num_rows-1].timestamp_us);
    return ESP_OK;
}

esp_err_t delete_csv_log(const char *filename) {
    if (filename == NULL) return ESP_ERR_INVALID_ARG;
    char path[256];
    snprintf(path, sizeof(path), "%s%s", LOG_PATH, filename);
    return storage_delete_file(path);
}

bool list_collected_csv_logs(FileInfo *files, size_t offset, size_t max_files, size_t *num_files) {
    return storage_list_files(LOG_PATH, ".csv", files, offset, max_files, num_files);
}