#ifndef TASK_MONITOR_H
#define TASK_MONITOR_H

#include <zephyr/kernel.h>

#define MAX_MONITORED_TASKS 16
#define TASK_DEADLINE_MISSED 1
#define TASK_STACK_OVERFLOW 2
#define TASK_RUNTIME_EXCEED 3

typedef enum {
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_SUSPENDED,
    TASK_STATE_TERMINATED
} task_state_t;

struct task_config {
    const char *name;
    uint32_t deadline_ms;
    uint32_t max_runtime_ms;
    size_t min_stack_remaining;
};

struct task_statistics {
    uint32_t total_runtime_ms;
    uint32_t context_switches;
    uint32_t stack_usage_peak;
    uint32_t cpu_usage_percent;
};

int task_monitor_init(void);
int register_monitored_task(const struct task_config *config);
void task_monitor_checkpoint(const char *task_name);
void task_monitor_start_execution(const char *task_name);
void task_monitor_end_execution(const char *task_name);
void update_task_state(const char *task_name, task_state_t new_state);
void get_task_statistics(const char *task_name, struct task_statistics *stats);

#endif /* TASK_MONITOR_H */
