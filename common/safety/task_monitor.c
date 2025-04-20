#include "task_monitor.h"
#include "error_handler.h"

struct monitored_task {
    struct task_config config;
    uint64_t last_checkpoint;
    uint64_t execution_start;
    uint32_t missed_deadlines;
    bool is_active;
    int execution_result1;
    int execution_result2;
    int execution_result3;
};

static struct monitored_task tasks[MAX_MONITORED_TASKS];
static uint8_t num_tasks = 0;

static void monitor_thread(void *p1, void *p2, void *p3) {
    while (1) {
        uint64_t current_time = k_uptime_get();
        
        for (int i = 0; i < num_tasks; i++) {
            if (!tasks[i].is_active) {
                continue;
            }
            
            // Check deadlines
            if (current_time - tasks[i].last_checkpoint > 
                tasks[i].config.deadline_ms) {
                handle_error(TASK_DEADLINE_MISSED);
                tasks[i].missed_deadlines++;
            }
            
            // Check runtime
            if (tasks[i].execution_start > 0 &&
                current_time - tasks[i].execution_start > 
                tasks[i].config.max_runtime_ms) {
                handle_error(TASK_RUNTIME_EXCEED);
            }
            
            // Check stack usage
            k_thread_runtime_stats_t stats;
            k_thread_runtime_stats_get(k_current_get(), &stats);
            if (stats.stack_size_free < tasks[i].config.min_stack_remaining) {
                handle_error(TASK_STACK_OVERFLOW);
            }
        }
        
        k_sleep(K_MSEC(10));  // Monitor period
    }
}

void task_monitor_checkpoint(const char *task_name) {
    for (int i = 0; i < num_tasks; i++) {
        if (strcmp(tasks[i].config.name, task_name) == 0) {
            // Update checkpoint time
            tasks[i].last_checkpoint = k_uptime_get();
            
            // Verify control flow sequence
            if (!verify_control_flow_sequence()) {
                handle_error(ERROR_CONTROL_FLOW_VIOLATION);
                enter_safe_state();
            }
            
            // Monitor stack usage
            k_thread_runtime_stats_t stats;
            k_thread_runtime_stats_get(k_current_get(), &stats);
            if (stats.stack_size_free < STACK_WARNING_THRESHOLD) {
                handle_error(ERROR_STACK_OVERFLOW_WARNING);
            }
            
            break;
        }
    }
}

// Add redundancy check for critical tasks
static bool verify_redundant_execution(struct monitored_task *task) {
    return (task->execution_result1 == task->execution_result2) &&
           (task->execution_result2 == task->execution_result3);
}

int task_monitor_init(void) {
    // Create monitor thread
    k_thread_create(&monitor_thread_data, monitor_stack,
                   K_THREAD_STACK_SIZEOF(monitor_stack),
                   monitor_thread,
                   NULL, NULL, NULL,
                   MONITOR_PRIORITY, 0, K_NO_WAIT);
    return 0;
}

int register_monitored_task(const struct task_config *config) {
    if (num_tasks >= MAX_MONITORED_TASKS) {
        return -ENOMEM;
    }
    
    tasks[num_tasks].config = *config;
    tasks[num_tasks].last_checkpoint = k_uptime_get();
    tasks[num_tasks].execution_start = 0;
    tasks[num_tasks].missed_deadlines = 0;
    tasks[num_tasks].is_active = true;
    tasks[num_tasks].execution_result1 = 0;
    tasks[num_tasks].execution_result2 = 0;
    tasks[num_tasks].execution_result3 = 0;
    
    num_tasks++;
    return 0;
}
