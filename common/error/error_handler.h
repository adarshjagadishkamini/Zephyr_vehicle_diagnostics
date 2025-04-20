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

void error_handler_init(void);
void handle_error(system_error_t error);
void watchdog_feed(void);

#endif /* ERROR_HANDLER_H */
