#include "asil_monitor.h"
#include "error_handler.h"

#define MAX_SAFETY_GOALS 16

static safety_goal_t safety_goals[MAX_SAFETY_GOALS];
static uint8_t num_goals = 0;

void register_safety_goal(safety_goal_t *goal) {
    if (num_goals < MAX_SAFETY_GOALS) {
        memcpy(&safety_goals[num_goals++], goal, sizeof(safety_goal_t));
    }
}

void monitor_safety_goals(void) {
    uint32_t current_time = k_uptime_get_32();
    
    for (int i = 0; i < num_goals; i++) {
        // Check goal timeout
        if (current_time - safety_goals[i].last_verified > safety_goals[i].timeout_ms) {
            handle_goal_violation(safety_goals[i].goal_id);
            continue;
        }
        
        // Verify goal
        if (!safety_goals[i].verify_func()) {
            handle_goal_violation(safety_goals[i].goal_id);
        } else {
            safety_goals[i].last_verified = current_time;
        }
    }
}

void handle_goal_violation(uint32_t goal_id) {
    for (int i = 0; i < num_goals; i++) {
        if (safety_goals[i].goal_id == goal_id && safety_goals[i].recovery_func) {
            safety_goals[i].recovery_func();
            return;
        }
    }
    
    // If no recovery function or recovery failed, enter safe state
    enter_safe_state();
}
