#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <zephyr/pm/pm.h>

typedef enum {
    POWER_STATE_ACTIVE,
    POWER_STATE_IDLE,
    POWER_STATE_STANDBY,
    POWER_STATE_OFF
} power_state_t;

void power_manager_init(void);
void set_power_state(power_state_t state);
void handle_wake_event(uint32_t event);
void configure_wake_sources(uint32_t sources);

#endif /* POWER_MANAGER_H */
