#include "control_flow.h"
#include "error_handler.h"

static uint32_t current_checkpoint = 0;
static uint32_t checkpoint_sequence[MAX_CHECKPOINTS];
static uint8_t sequence_idx = 0;

void control_flow_init(void) {
    memset(checkpoint_sequence, 0, sizeof(checkpoint_sequence));
    current_checkpoint = CF_INIT_START;
    sequence_idx = 0;
}

void checkpoint_reached(uint32_t checkpoint_id) {
    if (sequence_idx < MAX_CHECKPOINTS) {
        checkpoint_sequence[sequence_idx++] = checkpoint_id;
        current_checkpoint = checkpoint_id;
    }
}

bool verify_control_flow_sequence(void) {
    static const uint32_t valid_sequence[] = {
        CF_INIT_START,
        CF_SENSOR_READ,
        CF_CAN_SEND,
        CF_TASK_MONITOR,
        CF_ERROR_CHECK
    };
    
    for (int i = 0; i < sequence_idx; i++) {
        if (checkpoint_sequence[i] != valid_sequence[i]) {
            log_control_flow_violation();
            return false;
        }
    }
    return true;
}

void log_control_flow_violation(void) {
    static char msg[64];
    snprintf(msg, sizeof(msg), "Control flow violation at 0x%x", current_checkpoint);
    log_safety_event(msg, current_checkpoint);
}
