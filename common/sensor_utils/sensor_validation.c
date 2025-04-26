#include "sensor_validation.h"
#include <math.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor_validation, CONFIG_SENSOR_VALIDATION_LOG_LEVEL);

#define TOLERANCE 0.2f
#define VALIDATION_WINDOW 5
#define HISTORY_SIZE 10

struct sensor_history {
    float values[HISTORY_SIZE];
    uint32_t timestamps[HISTORY_SIZE];
    uint8_t index;
};

static struct sensor_history histories[MAX_SENSORS];

// Statistics tracking
struct sensor_statistics {
    uint32_t total_readings;
    uint32_t valid_readings;
    uint32_t range_violations;
    uint32_t rate_violations;
    uint32_t timeouts;
    float max_recorded_value;
    float min_recorded_value;
    float avg_value;
    float validation_success_rate;
    uint32_t last_error_timestamp;
};

static struct sensor_statistics sensor_stats[MAX_SENSORS];
static K_MUTEX_DEFINE(stats_mutex);

void get_sensor_statistics(uint8_t sensor_id, struct sensor_statistics *stats) {
    if (sensor_id >= MAX_SENSORS) {
        return;
    }
    
    k_mutex_lock(&stats_mutex, K_FOREVER);
    memcpy(stats, &sensor_stats[sensor_id], sizeof(struct sensor_statistics));
    k_mutex_unlock(&stats_mutex);
}

static void update_sensor_statistics(uint8_t sensor_id, float value, bool is_valid, uint8_t error_code) {
    if (sensor_id >= MAX_SENSORS) {
        return;
    }
    
    k_mutex_lock(&stats_mutex, K_FOREVER);
    struct sensor_statistics *stats = &sensor_stats[sensor_id];
    
    stats->total_readings++;
    if (is_valid) {
        stats->valid_readings++;
        
        // Update min/max/avg
        if (value < stats->min_recorded_value || stats->total_readings == 1) {
            stats->min_recorded_value = value;
        }
        if (value > stats->max_recorded_value || stats->total_readings == 1) {
            stats->max_recorded_value = value;
        }
        
        // Rolling average
        stats->avg_value = (stats->avg_value * (stats->valid_readings - 1) + value) / stats->valid_readings;
    } else {
        // Track error types
        switch (error_code) {
            case VALIDATION_RANGE_ERROR:
                stats->range_violations++;
                break;
            case VALIDATION_RATE_ERROR:
                stats->rate_violations++;
                break;
            case VALIDATION_TIMEOUT_ERROR:
                stats->timeouts++;
                break;
        }
        stats->last_error_timestamp = k_uptime_get_32();
    }
    
    // Update success rate
    stats->validation_success_rate = (float)stats->valid_readings / stats->total_readings * 100.0f;
    
    k_mutex_unlock(&stats_mutex);
}

const char *get_validation_error_string(uint8_t error_code) {
    switch (error_code) {
        case VALIDATION_SUCCESS:
            return "Success";
        case VALIDATION_RANGE_ERROR:
            return "Value out of range";
        case VALIDATION_RATE_ERROR:
            return "Rate of change exceeded";
        case VALIDATION_TIMEOUT_ERROR:
            return "Sensor timeout";
        case VALIDATION_NOISE_ERROR:
            return "Excessive noise";
        default:
            return "Unknown error";
    }
}

void reset_sensor_history(uint8_t sensor_id) {
    if (sensor_id >= MAX_SENSORS) {
        return;
    }
    
    k_mutex_lock(&stats_mutex, K_FOREVER);
    memset(&histories[sensor_id], 0, sizeof(struct sensor_history));
    memset(&sensor_stats[sensor_id], 0, sizeof(struct sensor_statistics));
    k_mutex_unlock(&stats_mutex);
}

void handle_sensor_timeout(uint8_t sensor_id) {
    LOG_ERR("Sensor %d timeout detected", sensor_id);
    update_sensor_statistics(sensor_id, 0.0f, false, VALIDATION_TIMEOUT_ERROR);
    reset_sensor_history(sensor_id);
}

bool validate_sensor_readings(sensor_triple_t *readings) {
    if (!readings->values_valid) {
        LOG_ERR("Invalid sensor values flag");
        return false;
    }
    
    // Check for timeout
    uint32_t current_time = k_uptime_get_32();
    if (current_time - readings->timestamp > SENSOR_TIMEOUT_MS) {
        LOG_ERR("Sensor readings timeout");
        readings->values_valid = false;
        return false;
    }
    
    // Validate using triple redundancy
    float diff12 = fabs(readings->primary_value - readings->secondary_value);
    float diff23 = fabs(readings->secondary_value - readings->reference_value);
    float diff13 = fabs(readings->primary_value - readings->reference_value);
    
    // All sensors agree within tolerance
    if (diff12 <= TOLERANCE && diff23 <= TOLERANCE && diff13 <= TOLERANCE) {
        return true;
    }
    
    // Two sensors agree within tolerance
    if (diff12 <= TOLERANCE) {
        readings->reference_value = (readings->primary_value + readings->secondary_value) / 2;
        LOG_WRN("Reference sensor disagreement corrected");
        return true;
    }
    
    if (diff23 <= TOLERANCE) {
        readings->primary_value = (readings->secondary_value + readings->reference_value) / 2;
        LOG_WRN("Primary sensor disagreement corrected");
        return true;
    }
    
    if (diff13 <= TOLERANCE) {
        readings->secondary_value = (readings->primary_value + readings->reference_value) / 2;
        LOG_WRN("Secondary sensor disagreement corrected");
        return true;
    }
    
    // All sensors disagree significantly
    LOG_ERR("All sensors disagree: P=%.2f, S=%.2f, R=%.2f", 
            readings->primary_value, readings->secondary_value, readings->reference_value);
    readings->values_valid = false;
    return false;
}

sensor_validation_result_t validate_sensor_reading(float value,
                                                 const sensor_validation_params_t *params,
                                                 uint64_t timestamp) {
    sensor_validation_result_t result = {
        .is_valid = true,
        .error_code = VALIDATION_SUCCESS
    };
    
    // Range check
    if (value < params->min_value || value > params->max_value) {
        LOG_WRN("Value %.2f outside range [%.2f, %.2f]", 
                value, params->min_value, params->max_value);
        result.is_valid = false;
        result.error_code = VALIDATION_RANGE_ERROR;
        update_sensor_statistics(params->sensor_id, value, false, VALIDATION_RANGE_ERROR);
        return result;
    }
    
    // Rate of change check using moving window
    struct sensor_history *history = &histories[params->sensor_id];
    uint8_t prev_idx = (history->index - 1) & (HISTORY_SIZE - 1);
    
    if (history->timestamps[prev_idx] > 0) {
        float time_diff = (timestamp - history->timestamps[prev_idx]) / 1000.0f;
        if (time_diff > 0) {
            float rate = fabs((value - history->values[prev_idx]) / time_diff);
            if (rate > params->max_rate_of_change) {
                LOG_WRN("Rate %.2f exceeds maximum %.2f", rate, params->max_rate_of_change);
                result.is_valid = false;
                result.error_code = VALIDATION_RATE_ERROR;
                update_sensor_statistics(params->sensor_id, value, false, VALIDATION_RATE_ERROR);
                return result;
            }
        }
    }
    
    // Timeout check
    if (history->timestamps[prev_idx] > 0 && 
        timestamp - history->timestamps[prev_idx] > params->timeout_ms) {
        LOG_WRN("Sensor timeout: %lld ms", 
                timestamp - history->timestamps[prev_idx]);
        result.is_valid = false;
        result.error_code = VALIDATION_TIMEOUT_ERROR;
        update_sensor_statistics(params->sensor_id, value, false, VALIDATION_TIMEOUT_ERROR);
        return result;
    }
    
    // Update history
    history->values[history->index] = value;
    history->timestamps[history->index] = timestamp;
    history->index = (history->index + 1) & (HISTORY_SIZE - 1);
    
    // Apply noise filtering using moving average
    float sum = 0;
    uint8_t count = 0;
    
    for (int i = 0; i < VALIDATION_WINDOW && i < HISTORY_SIZE; i++) {
        uint8_t idx = (history->index - 1 - i) & (HISTORY_SIZE - 1);
        if (history->timestamps[idx] > 0) {
            sum += history->values[idx];
            count++;
        }
    }
    
    if (count > 0) {
        result.filtered_value = sum / count;
    } else {
        result.filtered_value = value;
    }
    
    // Update statistics for valid reading
    update_sensor_statistics(params->sensor_id, result.filtered_value, true, VALIDATION_SUCCESS);
    
    return result;
}

float get_validated_sensor_value(sensor_triple_t *readings) {
    if (!readings->values_valid) {
        return 0.0f;
    }
    
    // Return average of valid readings
    if (fabs(readings->primary_value - readings->secondary_value) <= TOLERANCE) {
        return (readings->primary_value + readings->secondary_value) / 2;
    }
    
    if (fabs(readings->secondary_value - readings->reference_value) <= TOLERANCE) {
        return (readings->secondary_value + readings->reference_value) / 2;
    }
    
    if (fabs(readings->primary_value - readings->reference_value) <= TOLERANCE) {
        return (readings->primary_value + readings->reference_value) / 2;
    }
    
    // No agreement between sensors
    readings->values_valid = false;
    return 0.0f;
}

void handle_sensor_disagreement(const sensor_triple_t *readings) {
    LOG_ERR("Sensor disagreement detected:");
    LOG_ERR("  Primary: %.2f", readings->primary_value);
    LOG_ERR("  Secondary: %.2f", readings->secondary_value);
    LOG_ERR("  Reference: %.2f", readings->reference_value);
    
    // Log differences
    float diff12 = fabs(readings->primary_value - readings->secondary_value);
    float diff23 = fabs(readings->secondary_value - readings->reference_value);
    float diff13 = fabs(readings->primary_value - readings->reference_value);
    
    LOG_ERR("Differences: P-S=%.2f, S-R=%.2f, P-R=%.2f", diff12, diff23, diff13);
    
    // Determine most likely faulty sensor
    if (diff12 > TOLERANCE && diff13 > TOLERANCE) {
        LOG_ERR("Primary sensor likely faulty");
    } else if (diff12 > TOLERANCE && diff23 > TOLERANCE) {
        LOG_ERR("Secondary sensor likely faulty");
    } else if (diff13 > TOLERANCE && diff23 > TOLERANCE) {
        LOG_ERR("Reference sensor likely faulty");
    }
}
