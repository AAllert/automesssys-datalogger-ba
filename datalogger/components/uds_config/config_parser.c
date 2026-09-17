// implements
#include "config_parser.h"

// system includes
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"

// project includes
#include "storage.h"

#define TAG "config_parser"

// List of all columns which will be parsed into one config row. Structure: {<name_in_config>, <index_in_config>}
static config_column_t columns[] = {
    {"strLab", -1 }, 
    {"numIdTstr", -1 }, 
    {"numSid", -1 }, 
    {"numIdGtwy", -1 }, 
    {"numBytes", -1 },
    {"strDbc", -1 }, 
    {"numOfs", -1 },
    {"numFac", -1 },
    {"numDid", -1 },
    {"strType", -1 },
    {"stIntp", -1 },
    {"strName", -1 },
};
#define RELEVANT_COLUMN_COUNT (sizeof(columns) / sizeof(columns[0]))

/**
 * @brief Extracts and cleans the token at a given column index from a single CSV line.
 *
 * @note Mutates the passed line: commas up to and including the target column are replaced with '\0'.
 *
 * @param[in] line The CSV line to extract the token from.
 * @param[in] target_index The index of the column to extract.
 *
 * @return The cleaned token, or NULL if the line has fewer columns than target_index.
 */
static char *extract_column_token(char *line, int target_index) {
    if (line == NULL || target_index < 0) return NULL;

    int column_index = 0;
    char *cursor = line;

    while (cursor != NULL) {
        char *next_comma = strchr(cursor, ',');
        if (next_comma != NULL) {
            *next_comma = '\0';
        }
        if (column_index == target_index) {
            return clean_token(cursor);
        }
        column_index++;
        cursor = (next_comma != NULL) ? next_comma + 1 : NULL;
    }
    return NULL;
}

esp_err_t parse_config_file(const char *filepath, UdsConfig* config, int16_t *index_term15_row) {
    if (filepath == NULL || config == NULL) return ESP_ERR_INVALID_ARG;
    if (!file_exists(filepath)) return ESP_ERR_NOT_FOUND;

    const char *basename = strrchr(filepath, '/');
    config->filename = strdup(basename ? basename + 1 : filepath);
    config->rows = NULL;
    config->num_rows = 0;
    config->car_name = NULL;

    esp_err_t result = ESP_OK;
    *index_term15_row = -1;
    // Buffer for storage_read_line to store the last read line of the config file
    char *current_line = NULL;

    FILE *file = fopen(filepath, "r");
    if (!file) {
        result = ESP_FAIL;
        goto cleanup;
    }

    // read and parse Header
    if (!storage_read_line(file, &current_line)) {
        result = ESP_FAIL;
        goto cleanup;
    }
    if (find_columns_indices(current_line, columns, RELEVANT_COLUMN_COUNT) != ESP_OK) {
        result = ESP_FAIL;
        goto cleanup;
    }

    while (storage_read_line(file, &current_line)) {
        ConfigRow *tmp = realloc(config->rows, (config->num_rows + 1) * sizeof(ConfigRow));
        if (tmp == NULL) {
            result = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        config->rows = tmp;

        // The car name is stored redundantly on every row (strName), only capture it once.
        if (config->car_name == NULL) {
            char *line_copy = strdup(current_line);
            if (line_copy != NULL) {
                char *car_name_token = extract_column_token(line_copy, columns[STR_NAME].index);
                if (car_name_token != NULL && *car_name_token != '\0') {
                    config->car_name = strdup(car_name_token);
                }
                free(line_copy);
            }
        }

        if (parse_row(current_line, &config->rows[config->num_rows], columns, RELEVANT_COLUMN_COUNT) != ESP_OK) {
            result = ESP_FAIL;
            goto cleanup;
        }
        if (strcmp(config->rows[config->num_rows].name, "stTerm15") == 0) {
            *index_term15_row = (int16_t) config->num_rows;
        }
        config->num_rows++;
    }
    ESP_LOGI(TAG, "parsed rows: %d", config->num_rows);

cleanup: 
    if (file != NULL) fclose(file);
    free(current_line);
    return result;
}

esp_err_t find_columns_indices(char *header_line, config_column_t *columns, size_t num_columns) {
    if (header_line == NULL || columns == NULL || num_columns <= 0) return ESP_ERR_INVALID_ARG;

    // reset column indices to -1
    for (int i=0;i<num_columns;i++) {
        columns[i].index = -1;
    }

    int column_index = 0;
    char *cursor = header_line;

    while (cursor != NULL) {
        char *next_comma = strchr(cursor, ',');     // Manual split instead of strtok to recoginze empty fields
        if (next_comma != NULL) {
            *next_comma = '\0';
        }
        char *current_token = clean_token(cursor);
        for (int relevant_index = 0; relevant_index < num_columns; relevant_index++) {
            if (strcmp(current_token, columns[relevant_index].name) == 0) {
                columns[relevant_index].index = column_index;
            }
        }
        
        column_index++;
        cursor = (next_comma != NULL) ? next_comma + 1 : NULL;
    }

    // verify that all columns where found
    for (int i=0;i<num_columns;i++) {
        if (columns[i].index == -1) {
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_err_t parse_row(char *line, ConfigRow *row, config_column_t *columns, size_t num_columns) {
    if (line == NULL || row == NULL || columns == NULL || num_columns == 0) return ESP_ERR_INVALID_ARG;
    
    int column_index = 0;
    char *cursor = line;
    bool found[num_columns];
    memset(found, 0, sizeof(found));

    while (cursor != NULL) {
        char *next_comma = strchr(cursor, ',');     // Manual split instead of strtok to recoginze empty fields
        if (next_comma != NULL) {
            *next_comma = '\0';
        }
        char *current_token = clean_token(cursor);
        char *endptr;
        
        if (column_index == columns[STR_LAB].index) {
            if (*current_token == '\0') return ESP_FAIL;
            snprintf(row->name, sizeof(row->name), "%s", current_token);
            found[STR_LAB] = true;
        }
        else if (column_index == columns[NUM_ID_STR].index) {
            if (*current_token == '\0') return ESP_FAIL;
            row->canId = strtoul(current_token, &endptr, 0);
            if (endptr == current_token) return ESP_FAIL;
            found[NUM_ID_STR] = true;
        }
        else if (column_index == columns[NUM_SID].index) {
            if (*current_token == '\0') return ESP_FAIL;
            row->sid = strtoul(current_token, &endptr, 0);
            if (endptr == current_token) return ESP_FAIL;
            found[NUM_SID] = true;
        }
        else if (column_index == columns[NUM_ID_GTWY].index) {
            if (*current_token == '\0') return ESP_FAIL;
            row->responseCanId = strtoul(current_token, &endptr, 0);
            if (endptr == current_token) return ESP_FAIL;
            found[NUM_ID_GTWY] = true;
        }
        else if (column_index == columns[NUM_BYTES].index) {
            if (*current_token == '\0') return ESP_FAIL;
            row->numBytes = strtol(current_token, &endptr, 0);
            if (endptr == current_token) return ESP_FAIL;
            found[NUM_BYTES] = true;
        }
        else if (column_index == columns[STR_DBC].index) {
            if (*current_token == '\0') return ESP_FAIL;
            if (parse_dbc(current_token, &row->startBit, &row->length, &row->byte_order)) {
                found[STR_DBC] = true;
            } else {
                return ESP_FAIL;
            }
        }
        else if (column_index == columns[NUM_OFS].index) {
            if (*current_token == '\0') return ESP_FAIL;
            row->offset = strtof(current_token, &endptr);
            if (endptr == current_token) return ESP_FAIL;
            found[NUM_OFS] = true;
        }
        else if (column_index == columns[NUM_FAC].index) {
            if (*current_token == '\0') return ESP_FAIL;
            row->scale = strtod(current_token, &endptr);
            if (endptr == current_token) return ESP_FAIL;
            found[NUM_FAC] = true;
        }
        else if (column_index == columns[NUM_DID].index) {
            if (*current_token == '\0') return ESP_FAIL;
            uint16_t number = (uint16_t)strtoul(current_token, &endptr, 10);
            if (endptr == current_token) return ESP_FAIL;
            did_to_bytes(number, row->requestParameters);
            found[NUM_DID] = true;
        }
        else if (column_index == columns[STR_TYPE].index) {
            if (strcmp(current_token, "uint") == 0) {
                row->strType = UINT;
            } else if (strcmp(current_token, "int") == 0) {
                row->strType = INT;
            } else if (strcmp(current_token, "float32") == 0) {
                row->strType = FLOAT32;
            } else {
                return ESP_FAIL;
            }
            found[STR_TYPE] = true;
        }
        else if (column_index == columns[ST_INTP].index) {
            if (*current_token == '\0') return ESP_FAIL;
            row->should_interpolate = strtol(current_token, &endptr, 0) != 0;
            if (endptr == current_token) return ESP_FAIL;
            found[ST_INTP] = true;
        }

        column_index++;
        cursor = (next_comma != NULL) ? next_comma + 1 : NULL;
    }
    // This label is not parsed every row, so it is always true. 
    found[STR_NAME] = true;

    for (int i = 0; i < num_columns; i++) {
        if (!found[i]) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

char *clean_token(char *token)
{
    if (token == NULL) return NULL;
    if (*token == '\0') return token;

    // Skip leading whitespace and quotes
    while (*token == ' ' || *token == '"') {
        token++;
    }

    // Find end of string
    char *end = token + strlen(token) - 1;

    // Remove trailing whitespace, quotes and line endings
    while (end > token && (*end == ' ' || *end == '"' || *end == '\n' || *end == '\r')) {
        end--;
    }

    // Null terminate
    *(end + 1) = '\0';

    return token;
}

esp_err_t parse_metadata_for_config(const char *filepath, size_t *num_rows, char **car_name) {
    if (filepath == NULL || num_rows == NULL || car_name == NULL) return ESP_ERR_INVALID_ARG;
    if (!file_exists(filepath)) return ESP_ERR_NOT_FOUND;

    esp_err_t result = ESP_OK;
    char *current_line = NULL;
    *num_rows = 0;
    *car_name = NULL;

    FILE *file = fopen(filepath, "r");
    if (!file) {
        result = ESP_FAIL;
        goto cleanup;
    }

    // read and parse header, only to resolve the strName column index
    if (!storage_read_line(file, &current_line)) {
        result = ESP_FAIL;
        goto cleanup;
    }
    if (find_columns_indices(current_line, columns, RELEVANT_COLUMN_COUNT) != ESP_OK) {
        result = ESP_FAIL;
        goto cleanup;
    }

    while (storage_read_line(file, &current_line)) {
        if (*car_name == NULL) {
            char *line_copy = strdup(current_line);
            if (line_copy == NULL) {
                result = ESP_ERR_NO_MEM;
                goto cleanup;
            }
            char *car_name_token = extract_column_token(line_copy, columns[STR_NAME].index);
            if (car_name_token != NULL && *car_name_token != '\0') {
                *car_name = strdup(car_name_token);
            }
            free(line_copy);
        }
        (*num_rows)++;
    }

cleanup:
    if (file != NULL) fclose(file);
    free(current_line);
    return result;
}
