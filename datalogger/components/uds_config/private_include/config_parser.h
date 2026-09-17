#pragma once

// system includes
#include "esp_err.h"

// project includes
#include "uds_config.h"

typedef struct
{
    const char *name;
    int index;
} config_column_t;

/**
 * @brief Parses the UdsConfiguration.csv file containing the uds parameter configuration of the car into an array of ConfigRow structures. One row contains one uds parameter.
 *
 * The function:
 * 
 *  - Opens the specified car configuration CSV file
 * 
 *  - Reads and evaluates the header line
 * 
 *  - Detects relevant columns by name
 * 
 *  - Parses all subsequent rows
 * 
 *  - Allocates memory dynamically for the result array
 *
 * @param[in]  filepath Path to the CSV configuration file.
 * @param[out] config An UdsConfig struct containing all relevant data from the uds_config.csv file. 
 * @param[out] index_term_15_row The index of the row which holds the data for the status of the ignition. This is extra because the data is necessary to detect the start siognal for the datalogger.
 *
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_ARG if parameters are invalid
 * @return - ESP_ERR_NOT_FOUND if the file can not be opened
 * @return - ESP_ERR_NO_MEM if memory allocation fails
 * @return - ESP_ERR_INVALID_STATE if no valid rows were parsed
 * @return - ESP_FAIL if an unexpected error occurs
 *
 * @note - The function ignores malformed rows but continues parsing.
 * @note - CSV format must match expected column names.
 */
esp_err_t parse_config_file(const char *filepath, UdsConfig* config, int16_t *index_term15_row);

/**
 * @brief Helper function for parse_config_file, parses the header to find the indices of relevant columns.
 * 
 * Parses the config header for the strings in the columns array and saves the indices of the columns in the structs in the same array.
 * 
 * @param[in] header_line The header of the .csv file as a string. 
 * @param[inout] columns An array of config column structs which stores the relevant columns and the indices of the columns in the csv.
 * 
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_ARG if NULL or 0 is passed
 * @return - ESP_FAIL if not all indexes could be found
 */
esp_err_t find_columns_indices(char *header_line, config_column_t *columns, size_t num_columns);

/**
 * @brief helper function to parse one line into a config row.
 * 
 * @param[in] line One line of the config file, from which the data will be extracted
 * @param[out] row The parsed data in a config rwo struct
 * @param[in] columns The column array which holds the indexes of the relevant columns.
 * @param[in] num_columns The number of relevant columns
 * 
 * @return - ESP_OK on success 
 * @return - ESP_ERR_INVALID_ARG if NULL is passed to the function
 * @return - ESP_FAIL if the line could not be parsed
 */
esp_err_t parse_row(char *line, ConfigRow *row, config_column_t *columns, size_t num_columns);

/**
 * @brief removes all leading and trailing whitespaces and double quotes of a token.
 * 
 * @param[in] token The token which should be cleaned
 * 
 * @return The cleaned token
 */
char *clean_token(char *token);