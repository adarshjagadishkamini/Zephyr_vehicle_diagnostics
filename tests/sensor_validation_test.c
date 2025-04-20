#include <zephyr/ztest.h>
#include "sensor_validation.h"

ZTEST_SUITE(sensor_validation_tests, NULL, NULL, NULL, NULL, NULL);

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
}
