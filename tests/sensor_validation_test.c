#include <zephyr/ztest.h>
#include "sensor_validation.h"
#include "error_handler.h"
#include "asil.h"
#include "memory_protection.h"

ZTEST_SUITE(sensor_validation_tests, NULL, NULL, NULL, NULL, NULL);

// Test redundant sensor validation
ZTEST(sensor_validation_tests, test_triple_redundancy)
{
    sensor_triple_t readings = {
        .primary_value = 50.0f,
        .secondary_value = 50.2f,
        .reference_value = 49.9f,
        .timestamp = k_uptime_get(),
        .values_valid = true
    };
    
    zassert_true(validate_sensor_readings(&readings), "Valid redundant readings marked invalid");
    zassert_within(get_validated_sensor_value(&readings), 50.0f, 0.2f, "Incorrect voting result");

    // Test with one faulty sensor
    readings.secondary_value = 75.0f;
    zassert_true(validate_sensor_readings(&readings), "Valid 2-out-of-3 reading marked invalid");
    zassert_within(get_validated_sensor_value(&readings), 50.0f, 0.2f, "Incorrect fault tolerance");

    // Test with majority fault
    readings.primary_value = 75.0f;
    readings.reference_value = 75.0f;
    zassert_false(validate_sensor_readings(&readings), "Invalid majority not detected");
}

// Test range validation with ASIL-B requirements
ZTEST(sensor_validation_tests, test_range_validation)
{
    sensor_validation_params_t params = {
        .min_value = 0.0f,
        .max_value = 100.0f,
        .max_rate_of_change = 10.0f,
        .timeout_ms = 1000
    };

    sensor_validation_result_t result;
    
    // Test within range
    result = validate_sensor_reading(50.0f, &params, k_uptime_get());
    zassert_true(result.is_valid, "Valid reading marked invalid");

    // Test out of range
    result = validate_sensor_reading(150.0f, &params, k_uptime_get());
    zassert_false(result.is_valid, "Invalid reading marked valid");
    
    // Test rate of change violation
    k_sleep(K_MSEC(100));
    result = validate_sensor_reading(70.0f, &params, k_uptime_get());
    zassert_false(result.is_valid, "Rate violation not detected");
}

// Test timing and timeout handling
ZTEST(sensor_validation_tests, test_timing_validation)
{
    sensor_validation_params_t params = {
        .min_value = 0.0f,
        .max_value = 100.0f,
        .max_rate_of_change = 10.0f,
        .timeout_ms = 1000
    };

    sensor_validation_result_t result;
    uint64_t timestamp = k_uptime_get();
    
    result = validate_sensor_reading(50.0f, &params, timestamp);
    zassert_true(result.is_valid, "Initial reading invalid");
    
    // Test timeout
    timestamp += 2000; // Exceed timeout
    result = validate_sensor_reading(51.0f, &params, timestamp);
    zassert_false(result.is_valid, "Timeout not detected");
    zassert_equal(result.error_code, 3, "Wrong error code for timeout");
}

// Test error recovery mechanisms
ZTEST(sensor_validation_tests, test_error_recovery)
{
    sensor_triple_t readings = {
        .primary_value = 50.0f,
        .secondary_value = 50.0f,
        .reference_value = 50.0f,
        .timestamp = k_uptime_get(),
        .values_valid = true
    };
    
    // Simulate sensor failure and recovery
    readings.primary_value = 0.0f; // Primary sensor failure
    zassert_true(validate_sensor_readings(&readings), "Recovery from single sensor failure failed");
    
    readings.secondary_value = 0.0f; // Secondary sensor failure
    zassert_false(validate_sensor_readings(&readings), "Multiple sensor failure not detected");
    
    // Test recovery
    readings.primary_value = 50.0f;
    readings.secondary_value = 50.0f;
    zassert_true(validate_sensor_readings(&readings), "Recovery after multiple failures failed");
}

// Test memory protection for sensor data
ZTEST(sensor_validation_tests, test_memory_protection)
{
    // Set up protected memory region for sensor data
    sensor_triple_t protected_readings;
    int ret = protect_memory_region(&protected_readings, sizeof(sensor_triple_t), 
                                  PROT_READ | PROT_WRITE);
    zassert_equal(ret, 0, "Failed to set up memory protection");
    
    // Test legitimate access
    protected_readings.primary_value = 50.0f;
    zassert_equal(protected_readings.primary_value, 50.0f, "Valid access failed");
    
    // Verify memory integrity
    zassert_true(verify_memory_region(&protected_readings, sizeof(sensor_triple_t), 
                                    crc32_ieee(&protected_readings, sizeof(sensor_triple_t))), 
                 "Memory integrity check failed");
}

// Test safety monitoring
ZTEST(sensor_validation_tests, test_safety_monitoring)
{
    safety_state_t initial_state = SAFETY_STATE_NORMAL;
    
    // Test transition to degraded state
    sensor_triple_t readings = {
        .primary_value = 50.0f,
        .secondary_value = 0.0f, // Failed sensor
        .reference_value = 50.0f,
        .timestamp = k_uptime_get(),
        .values_valid = true
    };
    
    validate_sensor_readings(&readings);
    zassert_equal(get_safety_state(), SAFETY_STATE_DEGRADED, 
                 "Failed to transition to degraded state");
    
    // Test recovery
    readings.secondary_value = 50.0f;
    validate_sensor_readings(&readings);
    zassert_equal(get_safety_state(), SAFETY_STATE_NORMAL, 
                 "Failed to recover to normal state");
    
    // Test transition to safe state
    readings.primary_value = 0.0f;
    readings.secondary_value = 0.0f;
    validate_sensor_readings(&readings);
    zassert_equal(get_safety_state(), SAFETY_STATE_SAFE_STOP, 
                 "Failed to transition to safe state");
}

// Test load conditions
ZTEST(sensor_validation_tests, test_load_conditions)
{
    sensor_validation_params_t params = {
        .min_value = 0.0f,
        .max_value = 100.0f,
        .max_rate_of_change = 10.0f,
        .timeout_ms = 1000
    };

    // Test rapid validation requests
    for (int i = 0; i < 1000; i++) {
        sensor_validation_result_t result = validate_sensor_reading(
            50.0f + (i % 10), &params, k_uptime_get());
        zassert_true(result.is_valid, "Load test validation failed");
        k_sleep(K_MSEC(1));
    }
}
