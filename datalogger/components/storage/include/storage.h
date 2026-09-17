#pragma once

// system includes
#include <stdio.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_err.h"

// The set value from the sdkconfig under FatFS Options (LFN Heap must be enabled)
#define MAX_FILENAME_LENGTH CONFIG_FATFS_MAX_LFN
#define MAX_STORAGE_PATH_LENGTH 512

typedef struct {
   char name[MAX_FILENAME_LENGTH];
   size_t size;
} FileInfo;

/**
 * @brief Initilizes and mounts the SPI sd card module. 
 * 
 * This module is used to store the uds configs and the captured logs.
 * 
 * @param[in] pin_num_clk GPIO pin number for the SPI clock pin
 * @param[in] pin_num_cs GPIO pin number for the SPI chip select pin
 * @param[in] pin_num_miso GPIO pin number for Master in Slave out
 * @param[in] pin_num_mosi GPIO pin number for Master out Slave in
 * 
 * @return - ESP_OK on success
 * @return - ESP_FAIL if an unexpected error occurs
 */
esp_err_t sdcard_init(gpio_num_t pin_num_clk, gpio_num_t pin_num_cs, gpio_num_t pin_num_miso, gpio_num_t pin_num_mosi);

/**
 * @brief Unmounts the sd card for deepsleep mode. 
 */
void sdcard_deinit(void);

/**
 * @brief Initilizes and mounts the spiffs partition. 
 * 
 * @param[in] max_files Maximum files which can be stored in the spiffs partition.
 * 
 * The spiffs partition is used to store the files for the webserver and an initial config, that there is always an active config. 
 */
esp_err_t spiffs_init(size_t max_files);

/**
 * @brief Returns the usage of the sd card
 * 
 * If the sd card is not accessable, free and full will both return 0.
 * 
 * @param[out] free The space which is still free in MB. 
 * @param[out] used The space which is already used in MB. 
 * @param[out] total The space which is overall available in MB. 
 */
void get_sdcard_usage(uint64_t *free, uint64_t *used, uint64_t *total);

/**
 * @brief Creates a new file or overwrites an existing one with the passed content. 
 * 
 * @param[in] path The path including the filename, where the data should be written. 
 * @param[in] data The data which should be written to the file.
 * 
 * @return - ESP_OK on success
 * @return - ESP_ERR_NO_MEM if there is not enough memory available to complete the action. 
 * @return - ESP_FAIL if an unexpected error occurs
 */
esp_err_t storage_write_file(const char *path, const char *data);

/**
 * @brief Appends the passed data to the file. If the file does not exist, it will be created. 
 * 
 * @param[in] path The path to file, which should be updated. 
 * @param[in] data The data which should be written to the file. 
 * 
 * @return - ESP_OK on success
 * @return - ESP_ERR_NO_MEM if there is not enough memory available to complete the action. 
 * @return - ESP_FAIL if an unexpected error occurs. 
 */
esp_err_t storage_append_file(const char *path, const char *data);

/**
 * @brief Returns the content of the passed file. 
 * 
 * This funtions loads all content of the file into the RAM, which can be very expensive. 
 * If you expect a big file, it is better to use the stdio library to stream the content as shown below.
 * 
    \code
    FILE *f = fopen(<filepath>, "r"); 
    char chunk[512];
    size_t n;
    n = fread(chunk, 1, sizeof(chunk), f);
    fclose(f);
    \endcode
 * 
 * @param[in] path The path to the file which should be read.
 * @param[out] data A Double pointer to the content of the file. 
 * @param[out] size The size of the file content.
 * 
 * @return - ESP_OK on success
 * @return - ESP_ERR_NOT_FOUND if the file does not exist
 * @return - ESP_FAIL if an unexpected error occurs
 * 
 * @note The memory for the data is allocated by the function but must be freed by the caller. 
 */
esp_err_t storage_read_file(const char *path, char **data, size_t *size);

/**
 * @brief Reads one line from the passed file.
 * 
 * This function behaves like the getLine() function from POSIX systems. 
 * It reads always one line and the next time called it will read the next line of the file. 
 * 
 * 
 * @param[in] file A pointer to an already opened file. 
 * @param[out] line The read line.
 * 
 * @return - true if a line could successfully read
 * @return - false if no line could be read, e.g. the last read line was also the last line of the file.
 * 
 * @note This funtion is not thread safe. 
 * 
 * Example Usage to read a full file line by line
 * 
    \code
    FILE *f = fopen(<filepath>, "r"); 
    char *current_line = NULL;
    while (storage_read_line(file, &current_line)) {
      // Do something with the line
    }
    fclose(f);
    free(current_line)
    \endcode
 */
bool storage_read_line(FILE *file, char **line);

/**
 * @brief Deletes the file at the passed path. 
 * 
 * If the file does not exist, nothing will be deleted and ESP_OK returned. 
 * 
 * @param[in] path The path of the file which should be deleted. 
 * 
 * @return - ESP_OK on success
 * @return - ESP_FAIL if an unexpected error occurs
 */
esp_err_t storage_delete_file(const char *path);

/**
 * @brief Deletes the directory at the passed path.
 *
 * If the directory does not exist, ESP_ERR_NOT_FOUND is returned.
 *
 * @param[in] path The path of the directory which should be deleted.
 * @param[in] recursive Set this to true, if you want to delete the directory with all subdirectories and files inside.
 *
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_ARG if path is NULL
 * @return - ESP_ERR_NOT_FOUND if the directory does not exist
 * @return - ESP_ERR_INVALID_STATE if recursive is false and the directory is not empty
 * @return - ESP_FAIL if an unexpected error occurs
 */
esp_err_t storage_delete_directory(const char *path, bool recursive);

/**
 * @brief Checks, whether there exists a file or directory at the passed path.
 * 
 * @param[in] path The path which should be checked
 * 
 * @return - true if something exists at the path.
 * @return - false if the path could not be opened.
 */
bool file_exists(const char *path);

/**
 * @brief Checks, whether there exists a directory at the passed path.
 * 
 * @param[in] path The path which should be checked
 * 
 * @return - true if a directory exists at the path.
 * @return - false if the path could not be opened.
 */
bool storage_directory_exists(const char *path);

/**
 * @brief Maps a public-facing REST path to the corresponding absolute path on the SD card
 *
 * @note Supported paths: /configs/ and /logs/
 * @note Path traversal (..) is rejected
 *
 * @param[in] uri The request URI, e.g. "/configs/UdsConfig_v06_ID3.csv".
 * @param[out] path Buffer that receives the resolved absolute path on success.
 * @param[in] path_size Size of the path buffer.
 *
 * @return - true if the uri could be resolved into path
 * @return - false if the uri is not allowed or does not fit into the buffer
 */
bool storage_resolve_data_path(const char *uri, char *path, size_t path_size);

/**
 * @brief Lists all files of a directory with a specific extension.
 * 
 * @param[in] directory The diretory which contents should be listed. 
 * @param[in] extension The extesnion for which the files should be filtered. Pass NULL for no filter.
 * @param[out] files An array of structs containing information about all found files. 
 * @param[in] offset The number of mathcing files which will be skipped at the beginning
 * @param[in] max_files The maximum number of files which will be returned.
 * @param[out] file_count The number of files which were found and returned. 
 * 
 * @return - true if the directory exists, could be opened and the returned file list is valid.
 * @return - false if an error occured.
 */
bool storage_list_files(const char *directory, const char *extension, FileInfo *files, size_t offset, size_t max_files, size_t *file_count);