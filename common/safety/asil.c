#include <zephyr/kernel.h>
#include <string.h>
#include "asil.h"
#include "error_handler.h"
#include "memory_protection.h"
#include "task_monitor.h"

#define MONITOR_STACK_SIZE 2048
#define MONITOR_PRIORITY 2
#define MAX_EVENT_LOG_SIZE 100

struct safety_context {
    safety_state_t current_state;
    uint32_t monitor_counter;
    uint32_t last_monitor_time;
    bool control_flow_valid;
    uint8_t recovery_attempts;
    uint32_t last_recovery_time;
    struct safety_stats stats;
    K_MUTEX_DEFINE(stats_mutex);
};

// Control flow monitoring
static uint32_t control_flow_checkpoints[MAX_CHECKPOINTS];
static uint32_t expected_sequence[MAX_CHECKPOINTS];
static uint8_t num_checkpoints = 0;

// Safety event logging
struct safety_event {
    uint32_t timestamp;
    const char *event;
    uint32_t param;
};

K_MSGQ_DEFINE(safety_event_queue, sizeof(struct safety_event), MAX_EVENT_LOG_SIZE, 4);

// Static allocation
K_THREAD_STACK_DEFINE(monitor_stack, MONITOR_STACK_SIZE);
static struct k_thread monitor_thread;
static struct safety_context safety_ctx;

// Forward declarations
static void init_redundant_memory(void);
static void verify_critical_parameters(void);
static void process_safety_events(void);
static recovery_result_t execute_recovery_action(uint32_t error_type);

void safety_init(void) {
    memset(&safety_ctx, 0, sizeof(safety_ctx));
    safety_ctx.current_state = SAFETY_STATE_NORMAL;
    
    // Initialize redundancy checks
    init_redundant_memory();
    verify_critical_parameters();
    
    // Start safety monitor
    k_thread_create(&monitor_thread, monitor_stack,
                   MONITOR_STACK_SIZE,
                   (k_thread_entry_t)safety_monitor_task,
                   NULL, NULL, NULL,
                   MONITOR_PRIORITY, 0, K_NO_WAIT);
}

bool register_safety_checkpoint(uint32_t checkpoint_id, uint32_t expected_value) {
    if (num_checkpoints >= MAX_CHECKPOINTS) {
        return false;
    }
    
    expected_sequence[num_checkpoints] = expected_value;
    num_checkpoints++;
    return true;
}

void safety_monitor_task(void) {
    while (1) {
        // Process any queued safety events
        process_safety_events();
        
        // Check control flow
        if (!verify_control_flow()) {
            k_mutex_lock(&safety_ctx.stats_mutex, K_FOREVER);
            safety_ctx.stats.control_flow_violations++;
            k_mutex_unlock(&safety_ctx.stats_mutex);
            
            recovery_result_t result = attempt_recovery(ERROR_CONTROL_FLOW_VIOLATION);
            if (result != RECOVERY_RESULT_SUCCESS) {
                enter_safe_state();
            }
        }
        
        // Monitor task timing
        uint32_t current_time = k_uptime_get_32();
        if (current_time - safety_ctx.last_monitor_time > MAX_CRITICAL_TASK_TIME_MS) {
            k_mutex_lock(&safety_ctx.stats_mutex, K_FOREVER);
            safety_ctx.stats.timing_violations++;
            k_mutex_unlock(&safety_ctx.stats_mutex);
            handle_timing_violation();
        }
        
        // Check memory integrity
        if (!verify_memory_integrity()) {
            k_mutex_lock(&safety_ctx.stats_mutex, K_FOREVER);
            safety_ctx.stats.memory_violations++;
            k_mutex_unlock(&safety_ctx.stats_mutex);
            
            recovery_result_t result = attempt_recovery(ERROR_MEMORY_CORRUPTION);
            if (result != RECOVERY_RESULT_SUCCESS) {
                enter_safe_state();
            }
        }
        
        // Verify redundant variables
        if (!verify_redundant_variables()) {
            k_mutex_lock(&safety_ctx.stats_mutex, K_FOREVER);
            safety_ctx.stats.redundancy_mismatches++;
            k_mutex_unlock(&safety_ctx.stats_mutex);
            
            recovery_result_t result = attempt_recovery(ERROR_REDUNDANCY_MISMATCH);
            if (result != RECOVERY_RESULT_SUCCESS) {
                enter_safe_state();
            }
        }
        
        safety_ctx.last_monitor_time = current_time;
        k_sleep(K_MSEC(SAFETY_MONITOR_PERIOD_MS));
    }
}

recovery_result_t attempt_recovery(uint32_t error_type) {
    if (safety_ctx.recovery_attempts >= MAX_RECOVERY_ATTEMPTS) {
        k_mutex_lock(&safety_ctx.stats_mutex, K_FOREVER);
        safety_ctx.stats.failed_recoveries++;
        k_mutex_unlock(&safety_ctx.stats_mutex);
        return RECOVERY_RESULT_FAIL_SAFE;
    }
    
    uint32_t delay = RECOVERY_INITIAL_DELAY_MS;
    for (uint8_t i = 0; i < safety_ctx.recovery_attempts; i++) {
        delay *= RECOVERY_BACKOFF_FACTOR;
    }
    delay = MIN(delay, RECOVERY_MAX_DELAY_MS);
    
    safety_ctx.current_state = SAFETY_STATE_RECOVERY;
    k_sleep(K_MSEC(delay));
    
    recovery_result_t result = execute_recovery_action(error_type);
    safety_ctx.recovery_attempts++;
    
    if (result == RECOVERY_RESULT_SUCCESS) {
        k_mutex_lock(&safety_ctx.stats_mutex, K_FOREVER);
        safety_ctx.stats.successful_recoveries++;
        k_mutex_unlock(&safety_ctx.stats_mutex);
        safety_ctx.recovery_attempts = 0;
        safety_ctx.current_state = SAFETY_STATE_NORMAL;
    }
    
    return result;
}

static recovery_result_t execute_recovery_action(uint32_t error_type) {
    switch(error_type) {
        case ERROR_CONTROL_FLOW_VIOLATION:
            reset_safety_monitors();
            return verify_control_flow() ? RECOVERY_RESULT_SUCCESS : RECOVERY_RESULT_RETRY;
            
        case ERROR_MEMORY_CORRUPTION:
            if (restore_memory_backup()) {
                return RECOVERY_RESULT_SUCCESS;
            }
            return RECOVERY_RESULT_RETRY;
            
        case ERROR_REDUNDANCY_MISMATCH:
            if (restore_redundant_variables()) {
                return RECOVERY_RESULT_SUCCESS;
            }
            return RECOVERY_RESULT_SYSTEM_RESET;
            
        default:
            return RECOVERY_RESULT_FAIL_SAFE;
    }
}

void enter_safe_state(void) {
    safety_ctx.current_state = SAFETY_STATE_SAFE_STOP;
    
    // Disable all actuators
    disable_critical_outputs();
    
    // Notify VCU of safe state entry
    notify_safe_state_entry();
    
    // Log safety event
    log_safety_event("Entered safe state", 0);
    
    // Wait for system reset or manual intervention
    while (1) {
        k_sleep(K_MSEC(1000));
    }
}

bool verify_control_flow(void) {
    for (int i = 0; i < num_checkpoints; i++) {
        if (control_flow_checkpoints[i] != expected_sequence[i]) {
            return false;
        }
    }
    return true;
}

void log_safety_event(const char *event, uint32_t param) {
    struct safety_event evt = {
        .timestamp = k_uptime_get_32(),
        .event = event,
        .param = param
    };
    
    if (k_msgq_put(&safety_event_queue, &evt, K_NO_WAIT) != 0) {
        // Queue full, handle overflow
        k_msgq_purge(&safety_event_queue);
        k_msgq_put(&safety_event_queue, &evt, K_NO_WAIT);
    }
}

static void process_safety_events(void) {
    struct safety_event evt;
    while (k_msgq_get(&safety_event_queue, &evt, K_NO_WAIT) == 0) {
        // Process event (e.g., send to logging system)
        // Implementation depends on system requirements
    }
}

const struct safety_stats* get_safety_statistics(void) {
    return &safety_ctx.stats;
}

void reset_safety_monitors(void) {
    memset(control_flow_checkpoints, 0, sizeof(control_flow_checkpoints));
    safety_ctx.monitor_counter = 0;
    safety_ctx.control_flow_valid = true;
}
