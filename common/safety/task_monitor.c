#include <zephyr/kernel.h>
#include <string.h>
#include "task_monitor.h"
#include "error_handler.h"
#include "control_flow.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(task_monitor, CONFIG_TASK_MONITOR_LOG_LEVEL);

// Task monitoring context
struct monitored_task {
    struct task_config config;
    uint64_t last_checkpoint;
    uint64_t execution_start;
    uint32_t missed_deadlines;
    bool is_active;
    void *result1;
    void *result2;
    void *result3;
    task_state_t current_state;
    task_state_t last_state;
    struct task_statistics stats;
    K_MUTEX_DEFINE(stats_mutex);
};

static struct monitored_task tasks[MAX_MONITORED_TASKS];
static uint8_t num_tasks = 0;

// Monitor thread resources
K_THREAD_STACK_DEFINE(monitor_stack, MONITOR_STACK_SIZE);
static struct k_thread monitor_thread_data;
static K_MUTEX_DEFINE(monitor_mutex);

// Thread for CPU load monitoring
K_THREAD_STACK_DEFINE(cpu_monitor_stack, 1024);
static struct k_thread cpu_monitor_thread;
static volatile uint32_t system_cpu_load;

static void monitor_thread(void *p1, void *p2, void *p3) {
    while (1) {
        k_mutex_lock(&monitor_mutex, K_FOREVER);
        uint64_t current_time = k_uptime_get();
        
        for (int i = 0; i < num_tasks; i++) {
            if (!tasks[i].is_active) {
                continue;
            }
            
            // Check deadlines
            if (current_time - tasks[i].last_checkpoint > tasks[i].config.deadline_ms) {
                k_mutex_lock(&tasks[i].stats_mutex, K_FOREVER);
                tasks[i].missed_deadlines++;
                tasks[i].stats.deadline_misses++;
                k_mutex_unlock(&tasks[i].stats_mutex);

                if (tasks[i].missed_deadlines >= MAX_MISSED_DEADLINES) {
                    if (tasks[i].config.is_critical) {
                        enter_safe_state();
                    } else {
                        handle_task_error(tasks[i].config.name, TASK_DEADLINE_MISSED);
                    }
                }
            }
            
            // Check runtime
            if (tasks[i].execution_start > 0 &&
                current_time - tasks[i].execution_start > tasks[i].config.max_runtime_ms) {
                handle_task_error(tasks[i].config.name, TASK_RUNTIME_EXCEED);
            }
            
            // Check stack usage
            k_thread_runtime_stats_t runtime_stats;
            k_thread_runtime_stats_get(k_current_get(), &runtime_stats);
            
            k_mutex_lock(&tasks[i].stats_mutex, K_FOREVER);
            if (runtime_stats.stack_size_free < tasks[i].config.min_stack_remaining) {
                tasks[i].stats.stack_overflow = true;
                handle_task_error(tasks[i].config.name, TASK_STACK_OVERFLOW);
            }
            
            if (runtime_stats.stack_size_free < tasks[i].stats.stack_usage_peak) {
                tasks[i].stats.stack_usage_peak = runtime_stats.stack_size_free;
            }
            k_mutex_unlock(&tasks[i].stats_mutex);
        }
        
        k_mutex_unlock(&monitor_mutex);
        k_sleep(K_MSEC(10)); // Monitor period
    }
}

static void cpu_monitor_thread_entry(void *p1, void *p2, void *p3) {
    while (1) {
        uint32_t total_load = 0;
        k_mutex_lock(&monitor_mutex, K_FOREVER);
        
        for (int i = 0; i < num_tasks; i++) {
            if (tasks[i].is_active) {
                k_thread_runtime_stats_t stats;
                k_thread_runtime_stats_get(k_current_get(), &stats);
                tasks[i].stats.cpu_usage_percent = 
                    (stats.execution_cycles * 100) / k_cycle_get_32();
                total_load += tasks[i].stats.cpu_usage_percent;
            }
        }
        
        system_cpu_load = total_load;
        k_mutex_unlock(&monitor_mutex);
        
        if (system_cpu_load > CPU_OVERLOAD_THRESHOLD) {
            LOG_WRN("System CPU overload detected: %d%%", system_cpu_load);
        }
        
        k_sleep(K_MSEC(1000)); // Update CPU stats every second
    }
}

int task_monitor_init(void) {
    memset(tasks, 0, sizeof(tasks));
    
    // Start monitor thread
    k_thread_create(&monitor_thread_data,
                   monitor_stack,
                   K_THREAD_STACK_SIZEOF(monitor_stack),
                   monitor_thread,
                   NULL, NULL, NULL,
                   MONITOR_PRIORITY, 0, K_NO_WAIT);
                   
    // Start CPU monitoring thread
    k_thread_create(&cpu_monitor_thread,
                   cpu_monitor_stack,
                   K_THREAD_STACK_SIZEOF(cpu_monitor_stack),
                   cpu_monitor_thread_entry,
                   NULL, NULL, NULL,
                   MONITOR_PRIORITY + 1, 0, K_NO_WAIT);
                   
    control_flow_init();
    return 0;
}

int register_monitored_task(const struct task_config *config) {
    if (num_tasks >= MAX_MONITORED_TASKS) {
        return -ENOMEM;
    }
    
    k_mutex_lock(&monitor_mutex, K_FOREVER);
    
    tasks[num_tasks].config = *config;
    tasks[num_tasks].last_checkpoint = k_uptime_get();
    tasks[num_tasks].execution_start = 0;
    tasks[num_tasks].missed_deadlines = 0;
    tasks[num_tasks].is_active = true;
    tasks[num_tasks].current_state = TASK_STATE_INIT;
    tasks[num_tasks].last_state = TASK_STATE_INIT;
    memset(&tasks[num_tasks].stats, 0, sizeof(struct task_statistics));
    
    if (config->needs_redundancy) {
        tasks[num_tasks].result1 = k_malloc(256); // Fixed buffer for redundant results
        tasks[num_tasks].result2 = k_malloc(256);
        tasks[num_tasks].result3 = k_malloc(256);
    }
    
    num_tasks++;
    k_mutex_unlock(&monitor_mutex);
    return 0;
}

void verify_redundant_task_output(const char *task_name, void *result1, 
                                void *result2, void *result3, size_t size) {
    for (int i = 0; i < num_tasks; i++) {
        if (strcmp(tasks[i].config.name, task_name) == 0 && 
            tasks[i].config.needs_redundancy) {
            // Compare all three results
            if (memcmp(result1, result2, size) != 0 ||
                memcmp(result2, result3, size) != 0) {
                handle_task_error(task_name, TASK_CONTROL_FLOW_ERROR);
                return;
            }
            
            // Store results for later verification
            memcpy(tasks[i].result1, result1, size);
            memcpy(tasks[i].result2, result2, size);
            memcpy(tasks[i].result3, result3, size);
            break;
        }
    }
}

bool monitor_task_interference(const char *task_name) {
    for (int i = 0; i < num_tasks; i++) {
        if (strcmp(tasks[i].config.name, task_name) == 0) {
            k_thread_runtime_stats_t prev_stats, curr_stats;
            memcpy(&prev_stats, &tasks[i].last_runtime_stats, sizeof(k_thread_runtime_stats_t));
            k_thread_runtime_stats_get(k_current_get(), &curr_stats);
            
            // Check for interference from higher priority tasks
            if ((curr_stats.preempt_count - prev_stats.preempt_count) > 
                tasks[i].config.max_interference_count) {
                k_mutex_lock(&tasks[i].stats_mutex, K_FOREVER);
                tasks[i].stats.interference_count++;
                memcpy(&tasks[i].last_runtime_stats, &curr_stats, sizeof(k_thread_runtime_stats_t));
                k_mutex_unlock(&tasks[i].stats_mutex);
                
                // Log interference event
                LOG_WRN("Task interference detected on %s", task_name);
                log_task_violation(task_name, TASK_INTERFERENCE_ERROR);
                return true;
            }
            
            memcpy(&tasks[i].last_runtime_stats, &curr_stats, sizeof(k_thread_runtime_stats_t));
            break;
        }
    }
    return false;
}

void handle_task_error(const char *task_name, uint32_t error_type) {
    for (int i = 0; i < num_tasks; i++) {
        if (strcmp(tasks[i].config.name, task_name) == 0) {
            k_mutex_lock(&tasks[i].stats_mutex, K_FOREVER);
            
            switch (error_type) {
                case TASK_DEADLINE_MISSED:
                    tasks[i].stats.deadline_misses++;
                    break;
                case TASK_CONTROL_FLOW_ERROR:
                    tasks[i].stats.control_flow_violations++;
                    break;
                case TASK_INTERFERENCE_ERROR:
                    tasks[i].stats.interference_count++;
                    break;
            }
            
            tasks[i].stats.recovery_attempts++;
            tasks[i].current_state = TASK_STATE_ERROR;
            
            if (tasks[i].config.is_critical) {
                enter_safe_state();
            } else {
                recover_task_error(task_name);
            }
            
            k_mutex_unlock(&tasks[i].stats_mutex);
            break;
        }
    }
    
    log_task_violation(task_name, error_type);
}

bool recover_task_error(const char *task_name) {
    for (int i = 0; i < num_tasks; i++) {
        if (strcmp(tasks[i].config.name, task_name) == 0) {
            // Reset task state
            tasks[i].missed_deadlines = 0;
            tasks[i].execution_start = 0;
            tasks[i].current_state = TASK_STATE_READY;
            
            // Clear error flags
            tasks[i].stats.stack_overflow = false;
            
            // Attempt recovery based on configuration
            if (tasks[i].config.needs_redundancy) {
                // Switch to redundant implementation if available
                return true;
            }
            
            return false;
        }
    }
    return false;
}

void reset_task_monitoring(const char *task_name) {
    k_mutex_lock(&monitor_mutex, K_FOREVER);
    
    for (int i = 0; i < num_tasks; i++) {
        if (strcmp(tasks[i].config.name, task_name) == 0) {
            tasks[i].missed_deadlines = 0;
            tasks[i].execution_start = k_uptime_get();
            tasks[i].last_checkpoint = k_uptime_get();
            tasks[i].current_state = TASK_STATE_READY;
            memset(&tasks[i].stats, 0, sizeof(struct task_statistics));
            break;
        }
    }
    
    k_mutex_unlock(&monitor_mutex);
}

bool is_cpu_overloaded(void) {
    return system_cpu_load > CPU_OVERLOAD_THRESHOLD;
}

uint32_t get_task_safety_status(const char *task_name) {
    for (int i = 0; i < num_tasks; i++) {
        if (strcmp(tasks[i].config.name, task_name) == 0) {
            uint32_t status = 0;
            
            if (tasks[i].stats.deadline_misses > 0) status |= (1 << 0);
            if (tasks[i].stats.stack_overflow) status |= (1 << 1);
            if (tasks[i].stats.control_flow_violations > 0) status |= (1 << 2);
            if (tasks[i].stats.interference_count > 0) status |= (1 << 3);
            if (tasks[i].current_state == TASK_STATE_ERROR) status |= (1 << 4);
            
            return status;
        }
    }
    return 0xFFFFFFFF; // Invalid task
}
