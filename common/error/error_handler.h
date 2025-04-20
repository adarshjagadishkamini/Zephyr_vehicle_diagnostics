#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <zephyr/kernel.h>

typedef enum {
    ERROR_NONE = 0,
    ERROR_SENSOR_TIMEOUT,
    ERROR_CAN_BUS_OFF,
    ERROR_MQTT_DISCONNECT,
    ERROR_BLE_FAIL,
    ERROR_SECURE_BOOT
} system_error_t;

// ASIL-B specific error codes
#define ERROR_CONTROL_FLOW_VIOLATION  0x100
#define ERROR_STACK_OVERFLOW_WARNING  0x101
#define ERROR_REDUNDANCY_MISMATCH     0x102
#define ERROR_TIMING_VIOLATION        0x103
#define ERROR_MEMORY_CORRUPTION       0x104

// Error recovery actions
typedef enum {
    RECOVERY_RESET,
    RECOVERY_RETRY,
    RECOVERY_FALLBACK,
    RECOVERY_SAFE_STATE
} recovery_action_t;

// Error classification (ASIL-B requirements)
typedef enum {
    ERROR_CLASS_A,  // No safety impact
    ERROR_CLASS_B,  // Degraded operation possible
    ERROR_CLASS_C   // Immediate safe state required
} error_class_t;

void error_handler_init(void);
void handle_error(system_error_t error);
void watchdog_feed(void);
void handle_safety_error(uint32_t error_code, error_class_t class);
void log_safety_violation(const char *description, uint32_t data);

#endif /* ERROR_HANDLER_H */
