#ifndef ASIL_MONITOR_H
#define ASIL_MONITOR_H

// Safety goal verification
#define GOAL_VERIFY_PERIOD_MS 100

typedef struct {
    uint32_t goal_id;
    bool (*verify_func)(void);
    void (*recovery_func)(void);
    uint32_t timeout_ms;
    uint32_t last_verified;
} safety_goal_t;

void register_safety_goal(safety_goal_t *goal);
void monitor_safety_goals(void);
void handle_goal_violation(uint32_t goal_id);

#endif /* ASIL_MONITOR_H */
