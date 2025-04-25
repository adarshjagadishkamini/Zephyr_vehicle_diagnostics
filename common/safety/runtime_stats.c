#include <zephyr/kernel.h>
#include <string.h>
#include "runtime_stats.h"
#include "error_handler.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(runtime_stats, CONFIG_RUNTIME_STATS_LOG_LEVEL);

#define MAX_TASKS 32
#define HISTORY_SIZE 10
#define MOVING_AVG_WINDOW 5

struct task_runtime_data {
    char name[32];
    struct runtime_stats current;
    struct runtime_stats history[HISTORY_SIZE];
    uint32_t execution_times[MOVING_AVG_WINDOW];
    uint8_t history_index;
    uint8_t moving_avg_index;
    bool is_active;
    K_MUTEX_DEFINE(stats_mutex);
};

static struct task_runtime_data task_data[MAX_TASKS];
static uint8_t num_task_data = 0;
static K_MUTEX_DEFINE(runtime_mutex);

void runtime_stats_init(void) {
    k_mutex_lock(&runtime_mutex, K_FOREVER);
    memset(task_data, 0, sizeof(task_data));
    num_task_data = 0;
    k_mutex_unlock(&runtime_mutex);
}

static struct task_runtime_data *find_or_create_task(const char *task_name) {
    // First try to find existing task
    for (int i = 0; i < num_task_data; i++) {
        if (strcmp(task_data[i].name, task_name) == 0) {
            return &task_data[i];
        }
    }
    
    // Create new task entry if space available
    if (num_task_data < MAX_TASKS) {
        struct task_runtime_data *task = &task_data[num_task_data++];
        strncpy(task->name, task_name, sizeof(task->name) - 1);
        task->is_active = true;
        memset(&task->current, 0, sizeof(struct runtime_stats));
        task->history_index = 0;
        task->moving_avg_index = 0;
        return task;
    }
    
    return NULL;
}

void runtime_stats_start_task(const char *task_name) {
    k_mutex_lock(&runtime_mutex, K_FOREVER);
    struct task_runtime_data *task = find_or_create_task(task_name);
    
    if (task) {
        k_mutex_lock(&task->stats_mutex, K_FOREVER);
        task->current.start_time = k_uptime_get();
        task->current.execution_count++;
        k_mutex_unlock(&task->stats_mutex);
    }
    
    k_mutex_unlock(&runtime_mutex);
}

void runtime_stats_end_task(const char *task_name) {
    k_mutex_lock(&runtime_mutex, K_FOREVER);
    struct task_runtime_data *task = find_or_create_task(task_name);
    
    if (task) {
        k_mutex_lock(&task->stats_mutex, K_FOREVER);
        
        uint64_t end_time = k_uptime_get();
        uint32_t duration = end_time - task->current.start_time;
        
        // Update current stats
        task->current.total_runtime += duration;
        if (duration > task->current.max_execution_time) {
            task->current.max_execution_time = duration;
        }
        if (duration < task->current.min_execution_time || 
            task->current.min_execution_time == 0) {
            task->current.min_execution_time = duration;
        }
        
        // Update moving average
        task->execution_times[task->moving_avg_index] = duration;
        task->moving_avg_index = (task->moving_avg_index + 1) % MOVING_AVG_WINDOW;
        
        // Calculate average
        uint32_t sum = 0;
        for (int i = 0; i < MOVING_AVG_WINDOW; i++) {
            sum += task->execution_times[i];
        }
        task->current.avg_execution_time = sum / MOVING_AVG_WINDOW;
        
        // Store historical data periodically
        if (task->current.execution_count % 100 == 0) {
            memcpy(&task->history[task->history_index], 
                   &task->current, 
                   sizeof(struct runtime_stats));
            task->history_index = (task->history_index + 1) % HISTORY_SIZE;
        }
        
        k_mutex_unlock(&task->stats_mutex);
    }
    
    k_mutex_unlock(&runtime_mutex);
}

void runtime_stats_get(const char *task_name, struct runtime_stats *stats) {
    k_mutex_lock(&runtime_mutex, K_FOREVER);
    
    for (int i = 0; i < num_task_data; i++) {
        if (strcmp(task_data[i].name, task_name) == 0) {
            k_mutex_lock(&task_data[i].stats_mutex, K_FOREVER);
            memcpy(stats, &task_data[i].current, sizeof(struct runtime_stats));
            k_mutex_unlock(&task_data[i].stats_mutex);
            break;
        }
    }
    
    k_mutex_unlock(&runtime_mutex);
}

static void analyze_runtime_trend(const char *task_name) {
    for (int i = 0; i < num_task_data; i++) {
        if (strcmp(task_data[i].name, task_name) == 0) {
            k_mutex_lock(&task_data[i].stats_mutex, K_FOREVER);
            
            // Analyze execution time trend using historical data
            uint32_t prev_avg = 0;
            uint32_t trend_count = 0;
            
            for (int j = 0; j < HISTORY_SIZE; j++) {
                uint32_t curr_avg = task_data[i].history[j].avg_execution_time;
                if (prev_avg > 0 && curr_avg > prev_avg * 1.2) { // 20% increase
                    trend_count++;
                }
                prev_avg = curr_avg;
            }
            
            // Log warning if consistent upward trend detected
            if (trend_count >= HISTORY_SIZE/2) {
                LOG_WRN("Task %s showing increasing runtime trend", task_name);
            }
            
            k_mutex_unlock(&task_data[i].stats_mutex);
            break;
        }
    }
}

void runtime_stats_report(void) {
    k_mutex_lock(&runtime_mutex, K_FOREVER);
    
    LOG_INF("Runtime Statistics Report");
    LOG_INF("------------------------");
    
    for (int i = 0; i < num_task_data; i++) {
        if (!task_data[i].is_active) continue;
        
        k_mutex_lock(&task_data[i].stats_mutex, K_FOREVER);
        
        LOG_INF("Task: %s", task_data[i].name);
        LOG_INF("  Total Runtime: %lld ms", task_data[i].current.total_runtime);
        LOG_INF("  Executions: %d", task_data[i].current.execution_count);
        LOG_INF("  Max Time: %d ms", task_data[i].current.max_execution_time);
        LOG_INF("  Min Time: %d ms", task_data[i].current.min_execution_time);
        LOG_INF("  Avg Time: %d ms", task_data[i].current.avg_execution_time);
        
        // Analyze runtime trends
        analyze_runtime_trend(task_data[i].name);
        
        k_mutex_unlock(&task_data[i].stats_mutex);
    }
    
    k_mutex_unlock(&runtime_mutex);
}

bool runtime_stats_verify_history(const char *task_name) {
    bool result = true;
    
    k_mutex_lock(&runtime_mutex, K_FOREVER);
    
    for (int i = 0; i < num_task_data; i++) {
        if (strcmp(task_data[i].name, task_name) == 0) {
            k_mutex_lock(&task_data[i].stats_mutex, K_FOREVER);
            
            // Verify historical data integrity
            for (int j = 0; j < HISTORY_SIZE; j++) {
                if (task_data[i].history[j].execution_count > 0) {
                    if (task_data[i].history[j].min_execution_time > 
                        task_data[i].history[j].max_execution_time) {
                        result = false;
                        LOG_ERR("Invalid historical data detected for task %s", task_name);
                        break;
                    }
                }
            }
            
            k_mutex_unlock(&task_data[i].stats_mutex);
            break;
        }
    }
    
    k_mutex_unlock(&runtime_mutex);
    return result;
}
