#ifndef SENSOR_COMMON_H
#define SENSOR_COMMON_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/can.h>
#include "node_config.h"

#define SENSOR_THREAD_STACK_SIZE 2048
#define SENSOR_THREAD_PRIORITY 5

// Sensor reading status
typedef enum {
    SENSOR_READ_SUCCESS = 0,
    SENSOR_READ_ERROR,
    SENSOR_NOT_READY,
    SENSOR_TIMEOUT
} sensor_status_t;

// Common sensor initialization function
node_error_t sensor_init(const struct device *dev);

// Common CAN transmission function
int send_sensor_data(const struct device *can_dev, uint32_t id, 
                    const uint8_t *data, uint8_t len);

#endif /* SENSOR_COMMON_H */
