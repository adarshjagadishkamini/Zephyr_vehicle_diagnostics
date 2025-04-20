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

// ASIL-B sensor validation
typedef struct {
    float primary_value;
    float secondary_value;
    float reference_value;
    uint32_t timestamp;
    bool values_valid;
} sensor_triple_t;

bool validate_sensor_readings(sensor_triple_t *readings);
float get_validated_sensor_value(sensor_triple_t *readings);
void handle_sensor_disagreement(const sensor_triple_t *readings);

#endif /* SENSOR_VALIDATION_H */