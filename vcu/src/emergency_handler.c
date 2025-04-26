#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "emergency_handler.h"
#include "error_handler.h"
#include "asil.h"

LOG_MODULE_REGISTER(emergency_handler, CONFIG_EMERGENCY_LOG_LEVEL);

#define MAX_EMERGENCY_RETRIES 3
#define EMERGENCY_BACKOFF_MS 100

static atomic_t emergency_state = ATOMIC_INIT(EMERGENCY_STATE_NORMAL);
static uint32_t emergency_count = 0;
static K_MUTEX_DEFINE(emergency_mutex);

void handle_emergency_event(uint32_t event_type, const uint8_t *data, uint16_t len) {
    k_mutex_lock(&emergency_mutex, K_FOREVER);
    emergency_count++;
    
    // Log emergency event details
    LOG_ERR("Emergency event %d detected, count: %d", event_type, emergency_count);
    LOG_HEXDUMP_ERR(data, len, "Event data");
    
    // Update emergency state with atomic operation
    atomic_set(&emergency_state, EMERGENCY_STATE_ACTIVE);
    
    bool recovered = false;
    uint8_t retry_count = 0;
    
    while (!recovered && retry_count < MAX_EMERGENCY_RETRIES) {
        LOG_INF("Emergency recovery attempt %d", retry_count + 1);
        
        switch (event_type) {
            case EMERGENCY_COLLISION_RISK:
                recovered = handle_collision_risk(data, len);
                break;
                
            case EMERGENCY_BRAKE_FAILURE:
                recovered = handle_brake_failure(data, len);
                break;
                
            case EMERGENCY_SENSOR_ERROR:
                recovered = handle_sensor_emergency(data, len);
                break;
                
            case EMERGENCY_SYSTEM_ERROR:
                recovered = handle_system_emergency(data, len);
                break;
                
            case EMERGENCY_COMMUNICATION_ERROR:
                recovered = handle_communication_emergency(data, len);
                break;
                
            default:
                LOG_ERR("Unknown emergency event type: %d", event_type);
                k_mutex_unlock(&emergency_mutex);
                enter_safe_state();
                return;
        }
        
        if (!recovered) {
            retry_count++;
            // Exponential backoff between retries
            k_sleep(K_MSEC(EMERGENCY_BACKOFF_MS * (1 << retry_count)));
        }
    }
    
    if (recovered) {
        LOG_INF("Emergency event %d recovered after %d attempts", 
                event_type, retry_count + 1);
        atomic_set(&emergency_state, EMERGENCY_STATE_NORMAL);
        emergency_count = 0;
    } else {
        LOG_ERR("Failed to recover from emergency event %d after %d attempts", 
                event_type, MAX_EMERGENCY_RETRIES);
        k_mutex_unlock(&emergency_mutex);
        enter_safe_state();
        return;
    }
    
    k_mutex_unlock(&emergency_mutex);
}

static bool handle_collision_risk(const uint8_t *data, uint16_t len) {
    struct collision_data *collision = (struct collision_data *)data;
    
    // Validate data
    if (len < sizeof(struct collision_data)) {
        LOG_ERR("Invalid collision data size");
        return false;
    }
    
    LOG_WRN("Collision risk detected: distance=%d mm, velocity=%d mm/s",
            collision->distance, collision->relative_velocity);
    
    // Emergency brake activation
    if (activate_emergency_brake() != 0) {
        LOG_ERR("Failed to activate emergency brake");
        return false;
    }
    
    // Verify brake activation
    if (!verify_brake_state()) {
        LOG_ERR("Emergency brake activation verification failed");
        return false;
    }
    
    return true;
}

static bool handle_brake_failure(const uint8_t *data, uint16_t len) {
    struct brake_status *status = (struct brake_status *)data;
    
    if (len < sizeof(struct brake_status)) {
        LOG_ERR("Invalid brake status data size");
        return false;
    }
    
    LOG_ERR("Brake failure detected: pressure=%d kPa, status=0x%x",
            status->pressure, status->error_flags);
    
    // Try brake system reset
    if (reset_brake_system() != 0) {
        LOG_ERR("Brake system reset failed");
        return false;
    }
    
    // Switch to backup brake system if available
    if (status->backup_available) {
        if (activate_backup_brake() != 0) {
            LOG_ERR("Backup brake activation failed");
            return false;
        }
    }
    
    return verify_brake_redundancy();
}

static bool handle_sensor_emergency(const uint8_t *data, uint16_t len) {
    struct sensor_error *error = (struct sensor_error *)data;
    
    if (len < sizeof(struct sensor_error)) {
        LOG_ERR("Invalid sensor error data size");
        return false;
    }
    
    LOG_ERR("Critical sensor error: sensor_id=0x%x, error=0x%x",
            error->sensor_id, error->error_code);
    
    // Try sensor recovery
    if (recover_sensor(error->sensor_id) != 0) {
        // Switch to redundant sensor if available
        if (!switch_to_redundant_sensor(error->sensor_id)) {
            LOG_ERR("Failed to switch to redundant sensor");
            return false;
        }
    }
    
    return verify_sensor_readings(error->sensor_id);
}

static bool handle_system_emergency(const uint8_t *data, uint16_t len) {
    struct system_error *error = (struct system_error *)data;
    
    if (len < sizeof(struct system_error)) {
        LOG_ERR("Invalid system error data size");
        return false;
    }
    
    LOG_ERR("System emergency: component=0x%x, error=0x%x",
            error->component_id, error->error_code);
    
    // Try component reset
    if (reset_system_component(error->component_id) != 0) {
        LOG_ERR("Component reset failed");
        return false;
    }
    
    // Verify system state
    return verify_system_state(error->component_id);
}

static bool handle_communication_emergency(const uint8_t *data, uint16_t len) {
    struct comm_error *error = (struct comm_error *)data;
    
    if (len < sizeof(struct comm_error)) {
        LOG_ERR("Invalid communication error data size");
        return false;
    }
    
    LOG_ERR("Communication emergency: interface=0x%x, error=0x%x",
            error->interface_id, error->error_code);
    
    // Reset communication interface
    if (reset_comm_interface(error->interface_id) != 0) {
        LOG_ERR("Communication interface reset failed");
        return false;
    }
    
    // Try fallback communication path
    if (activate_comm_fallback(error->interface_id) != 0) {
        LOG_ERR("Fallback communication activation failed");
        return false;
    }
    
    return verify_communication_state(error->interface_id);
}

uint32_t get_emergency_state(void) {
    return atomic_get(&emergency_state);
}

uint32_t get_emergency_count(void) {
    k_mutex_lock(&emergency_mutex, K_FOREVER);
    uint32_t count = emergency_count;
    k_mutex_unlock(&emergency_mutex);
    return count;
}
