#include <zephyr/ztest.h>
#include "sensor_validation.h"
#include "error_handler.h"
#include "asil.h"
#include "memory_protection.h"

ZTEST_SUITE(sensor_validation_tests, NULL, NULL, NULL, NULL, NULL);

// Test basic sensor validation
ZTEST(sensor_validation_tests, test_basic_validation) {
    sensor_validation_params_t params = {
        .sensor_id = 0,
        .min_value = -50.0f,
        .max_value = 150.0f,
        .max_rate_of_change = 10.0f,
        .timeout_ms = 1000,
        .noise_threshold = 0.5f
    };
    
    // Test normal range
    sensor_validation_result_t result = validate_sensor_reading(25.0f, &params, k_uptime_get());
    zassert_true(result.is_valid, "Valid reading marked invalid");
    zassert_equal(result.error_code, VALIDATION_SUCCESS, "Unexpected error code");
    
    // Test out of range
    result = validate_sensor_reading(200.0f, &params, k_uptime_get());
    zassert_false(result.is_valid, "Invalid reading marked valid");
    zassert_equal(result.error_code, VALIDATION_RANGE_ERROR, "Wrong error code");
}

// Test rate of change validation
ZTEST(sensor_validation_tests, test_rate_validation) {
    sensor_validation_params_t params = {
        .sensor_id = 1,
        .min_value = 0.0f,
        .max_value = 100.0f,
        .max_rate_of_change = 10.0f,
        .timeout_ms = 1000
    };
    
    uint64_t time = k_uptime_get();
    
    // Initial reading
    sensor_validation_result_t result = validate_sensor_reading(50.0f, &params, time);
    zassert_true(result.is_valid, "Initial reading invalid");
    
    // Test acceptable rate
    time += 100;
    result = validate_sensor_reading(51.0f, &params, time);
    zassert_true(result.is_valid, "Valid rate marked invalid");
    
    // Test excessive rate
    time += 100;
    result = validate_sensor_reading(70.0f, &params, time);
    zassert_false(result.is_valid, "Excessive rate not detected");
    zassert_equal(result.error_code, VALIDATION_RATE_ERROR, "Wrong error code");
}

// Test timeout handling
ZTEST(sensor_validation_tests, test_timeout) {
    sensor_validation_params_t params = {
        .sensor_id = 2,
        .min_value = 0.0f,
        .max_value = 100.0f,
        .max_rate_of_change = 10.0f,
        .timeout_ms = 1000
    };
    
    uint64_t time = k_uptime_get();
    
    // Initial reading
    sensor_validation_result_t result = validate_sensor_reading(50.0f, &params, time);
    zassert_true(result.is_valid, "Initial reading invalid");
    
    // Test timeout
    time += 2000; // Exceed timeout
    result = validate_sensor_reading(51.0f, &params, time);
    zassert_false(result.is_valid, "Timeout not detected");
    zassert_equal(result.error_code, VALIDATION_TIMEOUT_ERROR, "Wrong error code");
}

// Test triple redundancy
ZTEST(sensor_validation_tests, test_triple_redundancy) {
    sensor_triple_t readings = {
        .primary_value = 50.0f,
        .secondary_value = 50.2f,
        .reference_value = 49.9f,
        .timestamp = k_uptime_get(),
        .values_valid = true
    };
    
    // Test all sensors valid
    zassert_true(validate_sensor_readings(&readings), 
                 "Valid redundant readings marked invalid");
    zassert_within(get_validated_sensor_value(&readings), 50.0f, 0.2f,
                  "Incorrect voting result");
    
    // Test with one faulty sensor
    readings.secondary_value = 75.0f;
    zassert_true(validate_sensor_readings(&readings),
                 "Valid 2-out-of-3 reading marked invalid");
    zassert_within(get_validated_sensor_value(&readings), 50.0f, 0.2f,
                  "Incorrect fault tolerance");
    
    // Test with majority fault
    readings.primary_value = 75.0f;
    readings.reference_value = 25.0f;
    zassert_false(validate_sensor_readings(&readings),
                 "Invalid majority not detected");
    zassert_false(readings.values_valid,
                 "Values still marked valid after majority fault");
}

// Test noise filtering
ZTEST(sensor_validation_tests, test_noise_filtering) {
    sensor_validation_params_t params = {
        .sensor_id = 3,
        .min_value = 0.0f,
        .max_value = 100.0f,
        .max_rate_of_change = 10.0f,
        .timeout_ms = 1000,
        .noise_threshold = 0.5f
    };
    
    uint64_t time = k_uptime_get();
    
    // Add several readings with noise
    float test_values[] = {50.0f, 50.3f, 49.8f, 50.2f, 49.9f};
    sensor_validation_result_t result;
    
    for (int i = 0; i < 5; i++) {
        result = validate_sensor_reading(test_values[i], &params, time + i * 100);
        zassert_true(result.is_valid, "Valid reading with noise marked invalid");
    }
    
    // Verify filtered value is close to true value
    zassert_within(result.filtered_value, 50.0f, 0.3f,
                  "Filtered value outside expected range");
}

// Test error handling
ZTEST(sensor_validation_tests, test_error_handling) {
    sensor_triple_t readings = {
        .primary_value = 50.0f,
        .secondary_value = 75.0f,
        .reference_value = 25.0f,
        .timestamp = k_uptime_get(),
        .values_valid = true
    };
    
    // Force disagreement
    validate_sensor_readings(&readings);
    
    // Test error string
    const char *error_str = get_validation_error_string(VALIDATION_RANGE_ERROR);
    zassert_not_null(error_str, "Error string not provided");
    zassert_true(strlen(error_str) > 0, "Empty error string");
}

// Test memory protection
ZTEST(sensor_validation_tests, test_memory_protection) {
    // Set up protected memory region for sensor data
    sensor_triple_t protected_readings;
    int ret = protect_memory_region(&protected_readings, 
                                  sizeof(sensor_triple_t),
                                  PROT_READ | PROT_WRITE);
    zassert_equal(ret, 0, "Failed to set up memory protection");
    
    // Test legitimate access
    protected_readings.primary_value = 50.0f;
    zassert_equal(protected_readings.primary_value, 50.0f,
                 "Valid memory access failed");
    
    // Verify memory integrity
    zassert_true(verify_memory_region(&protected_readings, 
                                    sizeof(sensor_triple_t),
                                    crc32_ieee(&protected_readings, 
                                             sizeof(sensor_triple_t))),
                 "Memory integrity check failed");
}

// Test load conditions
ZTEST(sensor_validation_tests, test_load_conditions) {
    sensor_validation_params_t params = {
        .sensor_id = 4,
        .min_value = 0.0f,
        .max_value = 100.0f,
        .max_rate_of_change = 10.0f,
        .timeout_ms = 1000
    };
    
    // Test rapid validation requests
    uint64_t time = k_uptime_get();
    for (int i = 0; i < 1000; i++) {
        sensor_validation_result_t result = validate_sensor_reading(
            50.0f + (i % 10), &params, time + i);
        zassert_true(result.is_valid, "Load test validation failed");
    }
}

// Test statistics collection
ZTEST(sensor_validation_tests, test_statistics) {
    sensor_validation_params_t params = {
        .sensor_id = 5,
        .min_value = 0.0f,
        .max_value = 100.0f,
        .max_rate_of_change = 10.0f,
        .timeout_ms = 1000
    };
    
    struct sensor_statistics stats;
    uint64_t time = k_uptime_get();
    
    // Generate some validation events
    for (int i = 0; i < 10; i++) {
        validate_sensor_reading(50.0f + i, &params, time + i * 100);
    }
    
    // Check statistics
    get_sensor_statistics(params.sensor_id, &stats);
    zassert_true(stats.total_readings > 0,
                 "Statistics not collected");
    zassert_true(stats.validation_success_rate > 0.0f,
                 "Success rate not calculated");
}

// Test concurrent validation
ZTEST(sensor_validation_tests, test_concurrent_validation) {
    sensor_validation_params_t params[3] = {
        {
            .sensor_id = 6,
            .min_value = 0.0f,
            .max_value = 100.0f,
            .max_rate_of_change = 10.0f,
            .timeout_ms = 1000
        },
        {
            .sensor_id = 7,
            .min_value = -50.0f,
            .max_value = 50.0f,
            .max_rate_of_change = 5.0f,
            .timeout_ms = 500
        },
        {
            .sensor_id = 8,
            .min_value = 0.0f,
            .max_value = 200.0f,
            .max_rate_of_change = 20.0f,
            .timeout_ms = 2000
        }
    };
    
    uint64_t time = k_uptime_get();
    
    // Validate multiple sensors concurrently
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 3; j++) {
            float value = 50.0f + (i % 10) * (j + 1);
            sensor_validation_result_t result = validate_sensor_reading(
                value, &params[j], time + i * 10);
            zassert_true(result.is_valid || result.error_code != VALIDATION_SUCCESS,
                        "Concurrent validation error");
        }
    }
}
