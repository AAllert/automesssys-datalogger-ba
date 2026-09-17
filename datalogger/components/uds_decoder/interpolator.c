// implements
#include "interpolator.h"

// system includes
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// project includes
#include "uds_config.h"

typedef struct
{
    char label[NAME_MAX_LENGTH + 1];
    double value;
    int64_t timestamp_us;
    bool has_value;
} cached_value_t;

static cached_value_t *cached_values = NULL;
static size_t cached_values_count = 0;
static int64_t end_of_last_iteration_timestamp_us;

static cached_value_t *find_cache_entry(const char *label) {
    for (size_t i = 0; i < cached_values_count; i++) {
        if (strcmp(cached_values[i].label, label) == 0) {
            return &cached_values[i];
        }
    }
    return NULL;
}

void interpolator_start_session(UdsConfig config) {
    free(cached_values);
    cached_values = NULL;
    cached_values_count = 0;
    end_of_last_iteration_timestamp_us = 0;

    cached_values = calloc(config.num_rows, sizeof(cached_value_t));
    if (cached_values == NULL) return;

    for (size_t i = 0; i < config.num_rows; i++) {
        strncpy(cached_values[i].label, config.rows[i].name, NAME_MAX_LENGTH);
        cached_values[i].label[NAME_MAX_LENGTH] = '\0';
        cached_values[i].has_value = false;
    }
    cached_values_count = config.num_rows;
}

bool interpolate(const char *label, int64_t timestamp_us, double data, double *result) {
    cached_value_t *entry = find_cache_entry(label);
    if (entry == NULL) return false;

    bool could_interpolate = entry->has_value;
    if (could_interpolate) {
        *result = linear_interpolate(timestamp_us, data, entry->timestamp_us, entry->value, end_of_last_iteration_timestamp_us);
    }

    entry->value = data;
    entry->timestamp_us = timestamp_us;
    entry->has_value = true;

    return could_interpolate;
}

void interpolator_set_target_timestamp(int64_t timestamp_us) {
    end_of_last_iteration_timestamp_us = timestamp_us;
}

int64_t interpolator_get_target_timestamp(void) {
    return end_of_last_iteration_timestamp_us;
}

double linear_interpolate(int64_t timestamp_us, double data, int64_t last_timestamp_us, double last_data, int64_t result_timestamp_us){
    if (timestamp_us == last_timestamp_us) return data;
    return (data - last_data) / (timestamp_us - last_timestamp_us) * (result_timestamp_us - last_timestamp_us) + last_data;
}
