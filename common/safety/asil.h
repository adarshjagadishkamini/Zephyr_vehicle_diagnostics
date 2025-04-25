#ifndef ASIL_H
#define ASIL_H

#include <zephyr/kernel.h>

// ASIL-B Safety Mechanisms
#define SAFETY_MONITOR_PERIOD_MS     100
#define WATCHDOG_TIMEOUT_MS         1000
#define MAX_CRITICAL_TASK_TIME_MS    50
#define MAX_CHECKPOINTS              16
#define MAX_RECOVERY_ATTEMPTS         3

// Safety Goal Categories
#define SAFETY_GOAL_BRAKE_SYSTEM    0x1000
#define SAFETY_GOAL_STEERING        0x2000
#define SAFETY_GOAL_ACCELERATION    0x3000
#define SAFETY_GOAL_COMMUNICATION   0x4000
#define SAFETY_GOAL_SENSOR_FUSION   0x5000

// Recovery Timing Parameters
#define RECOVERY_INITIAL_DELAY_MS    100
#define RECOVERY_MAX_DELAY_MS       2000
#define RECOVERY_BACKOFF_FACTOR        2

typedef enum {
    SAFETY_STATE_NORMAL = 0,
    SAFETY_STATE_DEGRADED,
    SAFETY_STATE_RECOVERY,
    SAFETY_STATE_SAFE_STOP
} safety_state_t;

typedef enum {
    RECOVERY_RESULT_SUCCESS = 0,
    RECOVERY_RESULT_RETRY,
    RECOVERY_RESULT_FAIL_SAFE,
    RECOVERY_RESULT_SYSTEM_RESET
} recovery_result_t;

struct safety_stats {
    uint32_t control_flow_violations;
    uint32_t timing_violations;
    uint32_t memory_violations;
    uint32_t redundancy_mismatches;
    uint32_t successful_recoveries;
    uint32_t failed_recoveries;
};

// Function prototypes
void safety_init(void);
void safety_monitor_task(void);
void enter_safe_state(void);
bool verify_control_flow(void);
void log_safety_event(const char *event, uint32_t param);
recovery_result_t attempt_recovery(uint32_t error_type);
bool verify_redundant_variables(void);
bool verify_memory_integrity(void);
void handle_timing_violation(void);
const struct safety_stats* get_safety_statistics(void);
bool register_safety_checkpoint(uint32_t checkpoint_id, uint32_t expected_value);
void reset_safety_monitors(void);

#endif /* ASIL_H */
