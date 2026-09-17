#pragma once

// system includes
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// project includes
#include "storage.h"

#define NAME_MAX_LENGTH 31
#define SIGNAL_ENCODING_MAX_LENGTH 31

#define CONFIG_PATH "/sdcard/configs/"

typedef enum {
    UINT,
    INT,
    FLOAT32
} datatype_t;

/**
 * @struct ConfigRow
 * @brief Represents one configuration entry parsed from the CSV file.
 *
 * Each row corresponds to a single diagnostic request configuration including CAN identifiers, service information and signal decoding data.
 */
typedef struct
{
    char name[NAME_MAX_LENGTH + 1];                         // internal name (strLab)
    uint32_t canId;                                         // CAN Identifier (numIdTstr)
    uint32_t sid;                                           // Service Identifier (numSid)
    uint8_t requestParameters[2];                           // Request Data Parameters including DID as byte array (numDid)
    uint32_t responseCanId;                                 // CAN Identifier to listen for response (numIdGtwy)
    uint8_t numBytes;                                       // count of expected response bytes (numBytes)
    uint16_t startBit;                                      // The bit at which the relevant data starts (extracted from dbc)
    uint8_t length;                                         // the count of relevant bits (extracted from dbc)
    uint8_t byte_order;                                     // If the response is in Little Endian (1) or Big Endian (0) (extracted from dbc)
    datatype_t strType;                                     // The type the decoded output value should have (strType)
    uint32_t offset;                                        // Offset of the physical value (numOfs)
    double scale;                                           // scale factor of the physical value (numFac)
    bool should_interpolate;                                // defines, if the result should be interpolated or not (stIntp)
} ConfigRow;

/**
 * @struct UdsConfig
 * @brief Represents a full uds_config.csv file. 
 * @note Also inlcudes metadata displayed in the webapp.
 */
typedef struct {
    const char *filename;   // Filename of the uds config file, the memory must be freed by the caller.
    ConfigRow *rows;        // Pointer to the array, in which all config rows are stored. The memory must be freed by the caller.
    size_t num_rows;        // number of loaded config rows
    const char *car_name;   // The name of the car (strName column), the memory must be freed by the caller.
} UdsConfig;

typedef enum
{
    STR_LAB,
    NUM_ID_STR,
    NUM_SID,
    NUM_ID_GTWY,
    NUM_BYTES,
    STR_DBC,
    NUM_OFS,
    NUM_FAC,
    NUM_DID,
    STR_TYPE,
    ST_INTP,
    STR_NAME
} ColumnId;

/**
 * @brief Parses the active config file and keeps the result in memory. 
 * 
 * After that, the active config can be returned by get_active_config very fast. 
 * 
 * @return ESP_OK on success
 * @return ESP_FAIL if the config could not be parsed
 */
esp_err_t parse_active_config(void);

/**
 * @brief Parses a config file for metadata.
 *
 * @param[in] filepath the path of the file to parse.
 * @param[out] num_rows the count of uds parameters (data rows) in the config.
 * @param[out] car_name the name of the car the config is for (strName column). The memory must be freed by the caller.
 *
 * @return - ESP_OK on success
 * @return - ESP_ERR_INVALID_ARG if filepath, num_rows or car_name is NULL
 * @return - ESP_ERR_NOT_FOUND if the config does not exist
 * @return - ESP_FAIL if an unexpected error occurs.
 */
esp_err_t parse_metadata_for_config(const char *filepath, size_t *num_rows, char **car_name);

/**
 * @brief Returns the currently active config as an array of config rows and the filename of the config. 
 * 
 * If the currently active config is not loaded it will be parsed first. 
 * 
 * @param[out] config A deep copy fo the currently active uds config. 
 * 
 * @return - ESP_OK on success
 */
esp_err_t get_active_config(UdsConfig *config);

/**
 * @brief Returns the Row of the config which holds the data for the ignition (strLab = stTerm15)
 * 
 * If the currently active config is not loaded it will be parsed first
 * 
 * @param[out] row The Config Row for term15 status
 * 
 * @return - ESP_OK on success
 * @return - ESP_FAIL if an error occurs
 */
esp_err_t get_active_ignition_row(ConfigRow *row);

/**
 * @brief Returns the filename of the currently active config.
 * 
 * @param[out] filename The filename of the currently active uds config. 
 * 
 * @return - ESP_OK on success
 */
esp_err_t get_active_config_name(char **filename);

/**
 * @brief Sets the active uds_config.csv file.
 */
esp_err_t set_active_config(const char *filename);

/**
 * @brief Returns a list of all available uds config names stored on the sd card. 
 * 
 * @param[out] files The filenames with metadata for all available configs, the configRow array will be NULL.
 * @param [out] num_files The number of found configs 
 * @param[in] max_files The maximum files which will be returned
 * 
 * @return - true if there were files found
 * @return - false if an error occurs
 */
bool list_available_configs(FileInfo *files, size_t offset, size_t max_files, size_t *num_files);

/**
 * @brief Checks if there is a config with the passed filename available in storage.
 * 
 * @param[in] filename The filename of the config to check
 * 
 * @return - true if the config was found
 * @return - false otherwise
 */
bool is_config_available(char *filename);

/**
 * @brief Helper function to convert the raw DID value from the CSV file into the representation required by UDS.
 *
 * @param[in]  did      The DID to be converted.
 * @param[out] bytes    Buffer with space for 2 bytes.
 */
void did_to_bytes(uint16_t did, uint8_t bytes[2]);

/**
 * @brief Parses a dbc string to its single values
 * 
 * The string must have the format "strtByte|lenByte@byte_order".
 * 
 * @param[in] strDbc The dbc string from the uds_config.csv
 * @param[out] startBit
 * @param[out] length
 * @param[out] byte_order 0 for Big-Endian (Motorola), 1 for Little- Endian (Intel)
 * 
 * @return - true if the string could be parsed successfully
 * @return - false if the string had an invalid format
 */
bool parse_dbc(const char *strDbc, uint16_t *startbit, uint8_t *length, uint8_t *byte_order);
