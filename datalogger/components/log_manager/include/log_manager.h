#pragma once

// system includes
#include <stdbool.h>
#include "esp_err.h"

// project inlcudes
#include "storage.h"

#define MAX_DATA_LEN 256
#define MAX_SIGNAL_NAME_LENGTH 31

/**
 * @struct LogEntry
 * @brief struct to hold the data for one log entry
 */
typedef struct {
    int64_t timestamp_us;                   // The UNIX timestamp when the uds response was received in microseconds
    char name[MAX_SIGNAL_NAME_LENGTH + 1];  // The short name of the uds signal (strLab)
    uint16_t data_length;                   // The length of the received byte array
    uint8_t data[MAX_DATA_LEN];             // The reveived data as a raw byte array
} LogEntry;

/**
 * @struct Log
 * @brief struct to hold the data for one log file.
 */
typedef struct {
    const char *filename;           // The filename of the log file. The memory must be freed by the caller. 
    LogEntry *entries;              // Pointer to an array of all entries in this log. The memory must be freed by the caller. 
    size_t num_entries;             // Number of log entries in the log
} Log;

/**
 * @brief Begins a new logfile. 
 * 
 * If a file with the passed or generated name already exists, new logs will be appended to this file. 
 * 
 * All calls to append_to_log will be saved to this logfile until this funtion is called again. 
 * 
 * @param[in] filename The function takes optionally a filename. Pass NULL to create a filename based on the current system time. 
 * @param[in] car_name Name of the car, which data will be logged. Only takes into account, if filename is NULL.
 * 
 * @return - ESP_OK on success
 * @return - ESP_FAIL if an error occurs
 * 
 * @note This function keeps the file always open to avoid an overhead of fopen() and fclose() calls to increase the performance.
 */
esp_err_t start_new_log_file(const char *filename, const char *car_name);

/**
 * @brief Closes the currently opened logfile, if there is one open.
 */
void close_log_file();

/**
 * @brief Force writes all log entries in memory on the sd card.
 */
void flush_log();

/**
 * @brief Saves the passed logs to the passed file. 
 * 
 * If the file does not exist, it will be created. Otherwise, the logs will be appended to the existing one. 
 * 
 * @param log_entry The LogEntry which should be saved to the currently open logfile. 
 * 
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_ARG if log_entry is NULL
 * @return - ESP_ERR_INVALID_STATE if no logfile was opened before with start_new_log_file()
 * @return - ESP_ERR_NO_MEM if there is not enough memory on the sd card to save the logs
 */
esp_err_t append_to_log(LogEntry *log_entry);

/**
 * @brief Deletes a log file. 
 * 
 * @param[in] filename The name of the log file, which should be deleted.
 * 
 * @return - ESP_OK on success
 * @return - ESP_ERR_NOT_FOUND if the log file does not exist
 * @return - ESP_FAIL if an unexpected error occurs
 */
esp_err_t delete_log(const char *filename);

/**
 * @brief Returns a list of the filenames of all stored logs on the sd card. 
 * 
 * If no files were found, num_files will be 0 and ESP_OK will be returned.
 * 
 * @param[out] filenames A double pointer to an array of all found filenames
 * @param[out] num_files The number of found files listed in the array
 * 
 * @return - true if the log directory could be opened.
 * @return - false if an error occured.
 */
bool list_collected_logs(FileInfo *files, size_t offset, size_t max_files, size_t *num_files);