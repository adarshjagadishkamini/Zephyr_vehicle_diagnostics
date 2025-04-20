#include "power_manager.h"
#include <zephyr/pm/device.h>
#include <zephyr/pm/policy.h>

static power_state_t current_state = POWER_STATE_ACTIVE;
static uint32_t enabled_wake_sources = 0;

void power_manager_init(void) {
    pm_device_wakeup_enable(can_dev, true);
    pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE);
}

void set_power_state(power_state_t state) {
    switch (state) {
        case POWER_STATE_IDLE:
            pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE);
            break;
            
        case POWER_STATE_STANDBY:
            pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_RAM);
            break;
            
        case POWER_STATE_OFF:
            prepare_system_off();
            pm_state_force(0, &(struct pm_state_info){PM_STATE_SOFT_OFF, 0, 0});
            break;
    }
    current_state = state;
}

void handle_wake_event(uint32_t event) {
    switch (event) {
        case WAKE_CAN_MSG:
            if (current_state != POWER_STATE_ACTIVE) {
                set_power_state(POWER_STATE_ACTIVE);
                resume_can_operations();
            }
            break;
        case WAKE_TIMER:
            handle_timer_wakeup();
            break;
        case WAKE_EXTERNAL:
            handle_external_wakeup();
            break;
    }
}

void configure_wake_sources(uint32_t sources) {
    enabled_wake_sources = sources;
    
    // Configure CAN wake-up
    if (sources & WAKE_CAN_MSG) {
        pm_device_wakeup_enable(can_dev, true);
    }
    
    // Configure GPIO wake-up
    if (sources & WAKE_EXTERNAL) {
        configure_gpio_wakeup();
    }
}
