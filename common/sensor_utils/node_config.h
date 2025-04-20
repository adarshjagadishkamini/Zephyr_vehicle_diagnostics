#ifndef NODE_CONFIG_H
#define NODE_CONFIG_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>

// Common definitions
#define CAN_BITRATE         500000
#define CAN_SAMPLE_POINT    875
#define TASK_STACK_SIZE     1024
#define TASK_PRIORITY       5

// Error codes
typedef enum {
    NODE_SUCCESS = 0,
    NODE_ERROR_CAN_INIT,
    NODE_ERROR_SENSOR_INIT,
    NODE_ERROR_TASK_CREATE
} node_error_t;

// Base node configuration structure
typedef struct {
    const struct device *can_dev;
    const struct device *sensor_dev;
    struct can_filter filter;
} node_config_t;

#endif /* NODE_CONFIG_H */
