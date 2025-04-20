#include "sensor_validation.h"

static float last_value = 0.0f;
static uint64_t last_timestamp = 0;

sensor_validation_result_t validate_sensor_reading(float value, 
                                                 const sensor_validation_params_t *params,
                                                 uint64_t timestamp) {
    sensor_validation_result_t result = {
        .is_valid = true,
        .error_code = 0
    };

    // Range check
    if (value < params->min_value || value > params->max_value) {
        result.is_valid = false;
        result.error_code = 1;
        return result;
    }

    // Rate of change check
    if (last_timestamp > 0) {
        float time_diff = (timestamp - last_timestamp) / 1000.0f;
        float rate = (value - last_value) / time_diff;
        
        if (fabs(rate) > params->max_rate_of_change) {
            result.is_valid = false;
            result.error_code = 2;
            return result;
        }
    }

    // Add timeout check
    if (timestamp - last_timestamp > params->timeout_ms) {
        result.is_valid = false;
        result.error_code = 3;
        return result;
    }

    // Add noise filtering using moving average
    #define WINDOW_SIZE 5
    static float values[WINDOW_SIZE];
    static int value_index = 0;

    values[value_index] = value;
    value_index = (value_index + 1) % WINDOW_SIZE;

    float sum = 0;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        sum += values[i];
    }
    float filtered_value = sum / WINDOW_SIZE;

    // Update last values with filtered value
    last_value = filtered_value;
    last_timestamp = timestamp;

    return result;
}
