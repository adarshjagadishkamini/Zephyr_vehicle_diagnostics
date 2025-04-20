#ifndef ASIL_H
#define ASIL_H

#include <zephyr/kernel.h>

// ASIL-B Safety Mechanisms
#define SAFETY_MONITOR_PERIOD_MS 100
#define WATCHDOG_TIMEOUT_MS 1000
#define MAX_CRITICAL_TASK_TIME_MS 50

typedef enum {
    SAFETY_STATE_NORMAL = 0,
    SAFETY_STATE_DEGRADED,
    SAFETY_STATE_SAFE_STOP
} safety_state_t;

void safety_init(void);
void safety_monitor_task(void);
void enter_safe_state(void);
bool verify_control_flow(void);
void log_safety_event(const char *event, uint32_t param);

#endif /* ASIL_H */
