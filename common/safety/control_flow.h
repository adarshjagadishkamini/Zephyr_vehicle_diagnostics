#ifndef CONTROL_FLOW_H
#define CONTROL_FLOW_H

#define MAX_CHECKPOINTS 32

// Control flow monitoring
void control_flow_init(void);
void checkpoint_reached(uint32_t checkpoint_id);
bool verify_control_flow_sequence(void);
void log_control_flow_violation(void);

// Control flow verification points
#define CF_INIT_START    0x01
#define CF_SENSOR_READ   0x02
#define CF_CAN_SEND      0x04
#define CF_TASK_MONITOR  0x08
#define CF_ERROR_CHECK   0x10

#endif /* CONTROL_FLOW_H */
