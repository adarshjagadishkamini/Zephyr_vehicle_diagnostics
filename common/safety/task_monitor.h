#ifndef TASK_MONITOR_H
#define TASK_MONITOR_H

#include <zephyr/kernel.h>
#include "runtime_stats.h"
#include "asil.h"

#define MAX_MONITORED_TASKS 16
#define MONITOR_STACK_SIZE 2048
#define MONITOR_PRIORITY 2
#define STACK_WARNING_THRESHOLD 256
#define MAX_MISSED_DEADLINES 3
#define CPU_OVERLOAD_THRESHOLD 80

// Task monitoring errors
#define TASK_DEADLINE_MISSED     1
#define TASK_STACK_OVERFLOW      2
#define TASK_RUNTIME_EXCEED      3
#define TASK_CONTROL_FLOW_ERROR  4
#define TASK_STATE_ERROR         5
#define TASK_INTERFERENCE_ERROR  6

typedef enum {
    TASK_STATE_INIT,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_SUSPENDED,
    TASK_STATE_TERMINATED,
    TASK_STATE_ERROR
} task_state_t;

struct task_config {
    const char *name;
    uint32_t deadline_ms;
    uint32_t max_runtime_ms;
    size_t min_stack_remaining;
    uint32_t expected_cycle_time;
    bool is_critical;
    bool needs_redundancy;
};

struct task_statistics {
    uint32_t total_runtime_ms;
    uint32_t context_switches;
    uint32_t stack_usage_peak;
    uint32_t cpu_usage_percent;
    uint32_t deadline_misses;
    uint32_t control_flow_violations;
    uint32_t interference_count;
    uint32_t recovery_attempts;
    bool stack_overflow;
    task_state_t current_state;
};

// Core monitoring functions
int task_monitor_init(void);
int register_monitored_task(const struct task_config *config);
void task_monitor_checkpoint(const char *task_name);
void task_monitor_start_execution(const char *task_name);
void task_monitor_end_execution(const char *task_name);

// Task state management
void update_task_state(const char *task_name, task_state_t new_state);
task_state_t get_task_state(const char *task_name);
bool is_task_active(const char *task_name);

// Statistics and monitoring
void get_task_statistics(const char *task_name, struct task_statistics *stats);
bool check_task_deadlines(void);
bool verify_task_control_flow(const char *task_name);
uint32_t get_task_cpu_usage(const char *task_name);
bool is_cpu_overloaded(void);

// Error recovery
void reset_task_monitoring(const char *task_name);
void handle_task_error(const char *task_name, uint32_t error_type);
bool recover_task_error(const char *task_name);

// ASIL-B specific features
void verify_redundant_task_output(const char *task_name, void *result1, void *result2, void *result3, size_t size);
bool monitor_task_interference(const char *task_name);
void log_task_violation(const char *task_name, uint32_t violation_type);
uint32_t get_task_safety_status(const char *task_name);

#endif /* TASK_MONITOR_H */
