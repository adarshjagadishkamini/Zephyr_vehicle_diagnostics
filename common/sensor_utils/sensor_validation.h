#ifndef SENSOR_VALIDATION_H
#define SENSOR_VALIDATION_H

#include <zephyr/kernel.h>
#include <stdbool.h>

// Validation parameters
typedef struct {
    float min_value;
    float max_value;
    float max_rate_of_change;
    uint32_t timeout_ms;
} sensor_validation_params_t;

// Validation result
typedef struct {
    bool is_valid;
    uint8_t error_code;
} sensor_validation_result_t;

// Validate sensor reading
sensor_validation_result_t validate_sensor_reading(float value, 
                                                 const sensor_validation_params_t *params,
                                                 uint64_t timestamp);

#endif /* SENSOR_VALIDATION_H */
