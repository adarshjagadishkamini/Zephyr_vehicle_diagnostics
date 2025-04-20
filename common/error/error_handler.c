#include "error_handler.h"
#include <zephyr/drivers/watchdog.h>

#define WDT_TIMEOUT_MS 5000

static const struct device *wdt_dev;
static struct wdt_timeout_cfg wdt_config;

void error_handler_init(void) {
    wdt_dev = DEVICE_DT_GET(DT_ALIAS(watchdog0));
    
    wdt_config.window.min = 0;
    wdt_config.window.max = WDT_TIMEOUT_MS;
    wdt_config.callback = NULL;
    
    wdt_install_timeout(wdt_dev, &wdt_config);
    wdt_setup(wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);
}

void handle_error(system_error_t error) {
    switch(error) {
        case ERROR_SENSOR_TIMEOUT:
            // Attempt sensor reset
            break;
            
        case ERROR_CAN_BUS_OFF:
            // Attempt CAN recovery
            can_recover_from_bus_off(can_dev);
            break;
            
        case ERROR_MQTT_DISCONNECT:
            // Attempt MQTT reconnection
            mqtt_disconnect(client);
            k_sleep(K_MSEC(1000));
            mqtt_connect(client);
            break;
            
        case ERROR_BLE_FAIL:
            // Reset BLE stack
            bt_disable();
            k_sleep(K_MSEC(1000));
            bt_enable(NULL);
            break;
            
        case ERROR_SECURE_BOOT:
            // Critical error - system reset
            sys_reboot(SYS_REBOOT_COLD);
            break;
    }
}

void watchdog_feed(void) {
    wdt_feed(wdt_dev, 0);
}
