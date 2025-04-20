#include <zephyr/ztest.h>
#include "sensor_validation.h"
#include "sensor_common.h"

ZTEST_SUITE(sensor_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(sensor_tests, test_sensor_validation) {
    sensor_validation_params_t params = {
        .min_value = -50.0f,
        .max_value = 150.0f,
        .max_rate_of_change = 10.0f,
        .timeout_ms = 1000
    };
    
    sensor_validation_result_t result;
    
    // Test normal range
    result = validate_sensor_reading(25.0f, &params, k_uptime_get());
    zassert_true(result.is_valid, "Valid reading marked invalid");
    
    // Test out of range
    result = validate_sensor_reading(200.0f, &params, k_uptime_get());
    zassert_false(result.is_valid, "Invalid reading marked valid");
}
