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
    task_state_t current_state;
    task_state_t last_state;
    struct task_statistics stats;
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

void update_task_state(const char *task_name, task_state_t new_state) {
    struct k_thread *thread;
    k_tid_t curr_tid = k_current_get();
    
    for (int i = 0; i < num_tasks; i++) {
        if (strcmp(tasks[i].config.name, task_name) == 0) {
            // Update state
            tasks[i].current_state = new_state;
            
            // Log state transition
            LOG_DBG("Task %s state change: %d", task_name, new_state);
            
            // Check for invalid transitions
            if (is_invalid_transition(tasks[i].last_state, new_state)) {
                handle_error(ERROR_INVALID_TASK_STATE);
            }
            
            // Update statistics
            if (new_state == TASK_STATE_RUNNING) {
                tasks[i].stats.context_switches++;
            }
            
            tasks[i].last_state = new_state;
            break;
        }
    }
}

void get_task_statistics(const char *task_name, struct task_statistics *stats) {
    for (int i = 0; i < num_tasks; i++) {
        if (strcmp(tasks[i].config.name, task_name) == 0) {
            memcpy(stats, &tasks[i].stats, sizeof(struct task_statistics));
            break;
        }
    }
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
    tasks[num_tasks].current_state = TASK_STATE_INIT;
    tasks[num_tasks].last_state = TASK_STATE_INIT;
    memset(&tasks[num_tasks].stats, 0, sizeof(struct task_statistics));
    
    num_tasks++;
    return 0;
}
