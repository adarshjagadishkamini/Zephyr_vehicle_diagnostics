#include "runtime_stats.h"
#include <zephyr/kernel.h>

#define MAX_TASKS 16
static struct runtime_stats task_stats[MAX_TASKS];
static char *task_names[MAX_TASKS];
static uint8_t num_tasks = 0;

void runtime_stats_init(void) {
    memset(task_stats, 0, sizeof(task_stats));
}

void runtime_stats_start_task(const char *task_name) {
    int idx = -1;
    
    // Find existing task or add new one
    for (int i = 0; i < num_tasks; i++) {
        if (strcmp(task_names[i], task_name) == 0) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1 && num_tasks < MAX_TASKS) {
        idx = num_tasks++;
        task_names[idx] = k_malloc(strlen(task_name) + 1);
        strcpy(task_names[idx], task_name);
    }
    
    if (idx != -1) {
        task_stats[idx].start_time = k_uptime_get();
        task_stats[idx].execution_count++;
    }
}

void runtime_stats_end_task(const char *task_name) {
    for (int i = 0; i < num_tasks; i++) {
        if (strcmp(task_names[i], task_name) == 0) {
            uint64_t duration = k_uptime_get() - task_stats[i].start_time;
            task_stats[i].total_runtime += duration;
            
            if (duration > task_stats[i].max_execution_time) {
                task_stats[i].max_execution_time = duration;
            }
            if (duration < task_stats[i].min_execution_time || 
                task_stats[i].min_execution_time == 0) {
                task_stats[i].min_execution_time = duration;
            }
            
            task_stats[i].avg_execution_time = 
                task_stats[i].total_runtime / task_stats[i].execution_count;
            break;
        }
    }
}

void runtime_stats_report(void) {
    for (int i = 0; i < num_tasks; i++) {
        printk("Task: %s\n", task_names[i]);
        printk("  Total Runtime: %lld ms\n", task_stats[i].total_runtime);
        printk("  Executions: %d\n", task_stats[i].execution_count);
        printk("  Max Time: %d ms\n", task_stats[i].max_execution_time);
        printk("  Min Time: %d ms\n", task_stats[i].min_execution_time);
        printk("  Avg Time: %d ms\n", task_stats[i].avg_execution_time);
        printk("  Deadline Misses: %d\n", task_stats[i].deadline_misses);
    }
}
