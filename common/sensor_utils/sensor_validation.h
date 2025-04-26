#ifndef SENSOR_VALIDATION_H
#define SENSOR_VALIDATION_H

#include <zephyr/kernel.h>
#include <stdbool.h>

#define MAX_SENSORS 16
#define SENSOR_TIMEOUT_MS 1000

// Validation error codes
#define VALIDATION_SUCCESS       0
#define VALIDATION_RANGE_ERROR  1
#define VALIDATION_RATE_ERROR   2
#define VALIDATION_TIMEOUT_ERROR 3
#define VALIDATION_NOISE_ERROR  4

// Validation parameters
typedef struct {
    uint8_t sensor_id;
    float min_value;
    float max_value;
    float max_rate_of_change;
    uint32_t timeout_ms;
    float noise_threshold;
} sensor_validation_params_t;

// Validation result with filtered value
typedef struct {
    bool is_valid;
    uint8_t error_code;
    float filtered_value;
} sensor_validation_result_t;

// Triple redundancy validation
typedef struct {
    float primary_value;
    float secondary_value;
    float reference_value;
    uint32_t timestamp;
    bool values_valid;
} sensor_triple_t;

// Core validation functions
sensor_validation_result_t validate_sensor_reading(float value,
                                                 const sensor_validation_params_t *params,
                                                 uint64_t timestamp);

bool validate_sensor_readings(sensor_triple_t *readings);
float get_validated_sensor_value(sensor_triple_t *readings);

// Error handling
void handle_sensor_disagreement(const sensor_triple_t *readings);
void handle_sensor_timeout(uint8_t sensor_id);
void reset_sensor_history(uint8_t sensor_id);

// Diagnostic functions
const char *get_validation_error_string(uint8_t error_code);
void get_sensor_statistics(uint8_t sensor_id, struct sensor_statistics *stats);

#endif /* SENSOR_VALIDATION_H */
