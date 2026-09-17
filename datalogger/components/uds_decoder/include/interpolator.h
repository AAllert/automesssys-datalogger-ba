#pragma once

// system includes
#include <stdbool.h>
#include <stdint.h>

// project includes
#include "uds_config.h"

/**
 * @brief Starts a new interpolator session.
 * 
 * This will delete the signal cache and the next iteration will not be interpolated. 
 * 
 * @param[in] config The uds configuration is needed to 
 */
void interpolator_start_session(UdsConfig config);

/**
 * @brief Interpolates a value to the timestamp of the end of the last iteration.
 * 
 * @param[in] label The label of the signal to interpolate
 * @param[in] timestamp_us The timestamp of the received value
 * @param[in] data The decoded value of the signal
 * @param[out] result The interpolated value
 * 
 * @return - true, if the signal could interpolated
 * @return - false, if this was the first iteration and signal could not interpolated
 */
bool interpolate(const char *label, int64_t timestamp_us, double data, double *result);

/**
 * @brief Sets the target timestamp. 
 * 
 * The interpolator will interpolate the values to this timestamp. 
 * 
 * @param[in] timestamp_us The unix timestamp to the target time. 
 */
void interpolator_set_target_timestamp(int64_t timestamp_us);

/**
 * @brief Getter for the currently targeted interpolation timestamp.
 * 
 * @return The timestamp to which all values are currently interpolated.
 */
int64_t interpolator_get_target_timestamp(void);

/**
 * @brief Executes a linear interpolation with the passed values.
 * 
 * @note If the target time falls outside the specified range, an extrapolation is performed.
 *
 * @param[in] timestamp_us The timestamp of the received value
 * @param[in] data The decoded value of the signal
 * @param[in] last_timestamp_us The timestamp of the last value
 * @param[in] last_data The decoded value of the last measured value.
 * @param[in] result_timestamp_us The timestamp to which the signal should be interpolated.
 * 
 * @return The interpolated value.
 */
double linear_interpolate(int64_t timestamp_us, double data, int64_t last_timestamp_us, double last_data, int64_t result_timestamp_us);