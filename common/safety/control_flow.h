#ifndef CONTROL_FLOW_H
#define CONTROL_FLOW_H

#include <zephyr/kernel.h>

#define MAX_CHECKPOINTS 32
#define MAX_SEQUENCES 16
#define MAX_SEQUENCE_LENGTH 32

// Control flow monitoring error codes
#define CF_ERR_INVALID_SEQUENCE  1
#define CF_ERR_MISSED_CHECKPOINT 2
#define CF_ERR_OUT_OF_ORDER     3
#define CF_ERR_TIMEOUT          4
#define CF_ERR_REDUNDANCY       5

// Common control flow checkpoints
#define CF_INIT_START           0x01
#define CF_SENSOR_READ          0x02
#define CF_DATA_VALIDATE        0x03
#define CF_CAN_SEND            0x04
#define CF_TASK_MONITOR        0x05
#define CF_ERROR_CHECK         0x06
#define CF_REDUNDANCY_CHECK    0x07
#define CF_SAFETY_CHECK        0x08
#define CF_TASK_END            0x09

// Control flow sequence validation
struct control_flow_sequence {
    uint32_t sequence_id;
    uint32_t checkpoints[MAX_SEQUENCE_LENGTH];
    uint8_t length;
    uint32_t timeout_ms;
    bool is_critical;
};

// Core functionality
void control_flow_init(void);
void reset_control_flow_monitoring(void);
void checkpoint_reached(uint32_t checkpoint_id);
bool verify_control_flow_sequence(void);
void log_control_flow_violation(void);

// Sequence management
int register_control_flow_sequence(const struct control_flow_sequence *sequence);
bool start_sequence_monitoring(uint32_t sequence_id);
bool end_sequence_monitoring(uint32_t sequence_id);
void abort_sequence_monitoring(uint32_t sequence_id);

// ASIL-B specific features
bool verify_redundant_control_flow(void);
void set_control_flow_timeout(uint32_t timeout_ms);
uint32_t get_control_flow_violations(void);
bool is_sequence_critical(uint32_t sequence_id);
void handle_control_flow_violation(uint32_t violation_type);

#endif /* CONTROL_FLOW_H */
