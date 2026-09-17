#pragma once

// system includes
#include <stdbool.h>
#include "esp_err.h"

// project inlcudes
#include "storage.h"
#include "uds_config.h"
#include "uds.h"

#define MAX_DATA_LEN 256
#define MAX_SIGNAL_NAME_LENGTH 31

/**
 * @brief Begins a new logfile. 
 * 
 * If a file with the passed or generated name already exists, new logs will be appended to this file. 
 * 
 * All calls to append_to_log will be saved to this logfile until this funtion is called again. 
 * 
 * @param[in] filename The function takes optionally a filename. Pass NULL to create a filename based on the current system time. 
 * @param[in] config The corresponding config for the data which should be written later. 
 * 
 * @return - ESP_OK on success
 * @return - ESP_FAIL if an error occurs
 * 
 * @note This function keeps the file always open to avoid an overhead of fopen() and fclose() calls to increase the performance.
 */
esp_err_t start_new_csv_log_file(const char *filename, UdsConfig config);

/**
 * @brief Closes the currently opened logfile, if there is one open.
 */
void close_csv_log_file();

/**
 * @brief Force writes all log entries in memory on the sd card.
 */
void flush_csv_log();

/**
 * @brief Saves the passed UdsResponses to the current open log.csv file.
 * 
 * The passed UDS responses will be decoded and interpolated before saving.
 * If the file does not exist, it will be created. Otherwise, the logs will be appended to the existing one. 
 * 
 * @param[in] responses An array of UDS responses to save in the file. 
 * @param[in] config An UdsConfig which wraps all parameters neccessary for bit extraction, decoding and interpolation. 
 * 
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_ARG if log_entry is NULL
 * @return - ESP_ERR_INVALID_STATE if no logfile was opened before with start_new_log_file()
 * @return - ESP_ERR_NO_MEM if there is not enough memory on the sd card to save the logs
 */
esp_err_t append_csv_log_row(UdsResponse *responses, UdsConfig config);

/**
 * @brief Deletes a log file. 
 * 
 * @param[in] filename The name of the log file, which should be deleted.
 * 
 * @return - ESP_OK on success
 * @return - ESP_ERR_NOT_FOUND if the log file does not exist
 * @return - ESP_FAIL if an unexpected error occurs
 */
esp_err_t delete_csv_log(const char *filename);

/**
 * @brief Returns a list of the filenames of all stored logs.csv files on the sd card. 
 * 
 * If no files were found, num_files will be 0 and false will be returned.
 * 
 * @param[out] filenames A double pointer to an array of all found filenames
 * @param[out] num_files The number of found files listed in the array
 * 
 * @return - true if the log directory could be opened.
 * @return - false if an error occured.
 */
bool list_collected_csv_logs(FileInfo *files, size_t offset, size_t max_files, size_t *num_files);