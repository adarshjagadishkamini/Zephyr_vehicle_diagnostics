#include <zephyr/kernel.h>
#include "error_handler.h"
#include "asil.h"
#include "task_monitor.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(error_handler, CONFIG_ERROR_HANDLER_LOG_LEVEL);

#define MAX_RETRIES 3
#define RETRY_DELAY_MS 100

static uint32_t error_counts[ERROR_TYPE_MAX];
static K_MUTEX_DEFINE(error_mutex);

void error_handler_init(void) {
    k_mutex_lock(&error_mutex, K_FOREVER);
    memset(error_counts, 0, sizeof(error_counts));
    k_mutex_unlock(&error_mutex);
}

static bool should_attempt_recovery(uint32_t error_type) {
    k_mutex_lock(&error_mutex, K_FOREVER);
    bool result = error_counts[error_type] < MAX_RETRIES;
    if (result) {
        error_counts[error_type]++;
    }
    k_mutex_unlock(&error_mutex);
    return result;
}

static void reset_error_count(uint32_t error_type) {
    k_mutex_lock(&error_mutex, K_FOREVER);
    error_counts[error_type] = 0;
    k_mutex_unlock(&error_mutex);
}

void handle_error(uint32_t error_type) {
    LOG_ERR("Error detected: %d", error_type);
    
    if (!should_attempt_recovery(error_type)) {
        LOG_ERR("Max retries exceeded for error type %d", error_type);
        enter_safe_state();
        return;
    }

    bool recovered = false;
    switch (error_type) {
        case ERROR_CAN_BUS:
            recovered = handle_can_bus_error();
            break;
            
        case ERROR_SENSOR:
            recovered = handle_sensor_error();
            break;
            
        case ERROR_TASK_DEADLINE:
            recovered = handle_task_deadline_miss();
            break;
            
        case ERROR_STACK_OVERFLOW:
            recovered = handle_stack_overflow();
            break;
            
        case ERROR_MEMORY_CORRUPTION:
            recovered = handle_memory_violation();
            break;
            
        case ERROR_SECURE_BOOT:
            // Critical error - no recovery
            LOG_ERR("Critical secure boot error");
            enter_safe_state();
            return;
            
        default:
            LOG_ERR("Unknown error type: %d", error_type);
            enter_safe_state();
            return;
    }
    
    if (recovered) {
        reset_error_count(error_type);
        LOG_INF("Successfully recovered from error %d", error_type);
    } else {
        LOG_ERR("Recovery failed for error %d", error_type);
        if (!should_attempt_recovery(error_type)) {
            enter_safe_state();
        }
    }
}

static bool handle_can_bus_error(void) {
    LOG_WRN("Attempting CAN bus recovery");
    // Try to recover bus-off state
    if (can_recover_from_bus_off(can_dev) != 0) {
        return false;
    }
    
    // Reset CAN controllers
    can_stop(can_dev);
    k_sleep(K_MSEC(100));
    if (can_start(can_dev) != 0) {
        return false;
    }
    
    // Reset message counters and buffers
    reset_can_statistics();
    return true;
}

static bool handle_sensor_error(void) {
    LOG_WRN("Attempting sensor recovery");
    // Try sensor reinitialization
    if (reinitialize_sensors() != 0) {
        return false;
    }
    
    // Verify sensor readings
    if (!verify_sensor_readings()) {
        return false;
    }
    
    return true;
}

static bool handle_task_deadline_miss(void) {
    LOG_WRN("Handling task deadline miss");
    // Reset affected task
    reset_violated_task();
    
    // Check system load
    if (is_cpu_overloaded()) {
        LOG_WRN("System overload detected");
        // Attempt load reduction
        if (!reduce_system_load()) {
            return false;
        }
    }
    
    return true;
}

static bool handle_stack_overflow(void) {
    LOG_ERR("Stack overflow detected");
    // Stack overflow is critical - terminate affected task
    terminate_violated_task();
    
    // Verify remaining stack
    if (!verify_stack_integrity()) {
        return false;
    }
    
    return true;
}

static bool handle_memory_violation(void) {
    LOG_ERR("Memory violation detected");
    void *corrupted_addr = get_last_fault_address();
    
    // Check if address is in protected region
    if (is_critical_memory_region(corrupted_addr)) {
        LOG_ERR("Critical memory region corrupted");
        return false;
    }
    
    // Try to restore from backup
    if (restore_memory_backup()) {
        if (verify_memory_integrity()) {
            return true;
        }
    }
    
    return false;
}

static bool reduce_system_load(void) {
    // Identify non-critical tasks
    struct task_statistics stats;
    bool reduced = false;
    
    for (int i = 0; i < MAX_MONITORED_TASKS; i++) {
        const char *task_name = get_task_name(i);
        if (!task_name) continue;
        
        get_task_statistics(task_name, &stats);
        if (stats.cpu_usage_percent > 20 && !is_critical_task(task_name)) {
            // Reduce task priority or frequency
            if (reduce_task_priority(task_name)) {
                reduced = true;
            }
        }
    }
    
    return reduced;
}

static bool verify_stack_integrity(void) {
    // Check stack guards
    for (int i = 0; i < MAX_MONITORED_TASKS; i++) {
        const char *task_name = get_task_name(i);
        if (!task_name) continue;
        
        if (!verify_stack_guards(task_name)) {
            return false;
        }
    }
    return true;
}

void notify_error_recovery(uint32_t error_type) {
    k_mutex_lock(&error_mutex, K_FOREVER);
    error_counts[error_type] = 0;
    k_mutex_unlock(&error_mutex);
    
    LOG_INF("Error %d recovery confirmed", error_type);
}

uint32_t get_error_count(uint32_t error_type) {
    if (error_type >= ERROR_TYPE_MAX) {
        return 0;
    }
    
    k_mutex_lock(&error_mutex, K_FOREVER);
    uint32_t count = error_counts[error_type];
    k_mutex_unlock(&error_mutex);
    
    return count;
}
