#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/can.h>
#include "sensor_common.h"
#include "can_ids.h"

#define INA219_ADDR 0x40
#define INA219_CONFIG_REG 0x00
#define INA219_CURRENT_REG 0x04
#define INA219_VOLTAGE_REG 0x02

static const struct device *i2c_dev;
static const struct device *can_dev;

static void battery_thread(void *arg1, void *arg2, void *arg3)
{
    uint8_t can_data[BATTERY_MSG_LEN];
    int16_t current_raw, voltage_raw;
    float voltage, current;

    while (1) {
        // Read voltage and current from INA219
        i2c_burst_read(i2c_dev, INA219_ADDR, INA219_VOLTAGE_REG, (uint8_t *)&voltage_raw, 2);
        i2c_burst_read(i2c_dev, INA219_ADDR, INA219_CURRENT_REG, (uint8_t *)&current_raw, 2);

        voltage = voltage_raw * 0.00125f; // LSB = 1.25mV
        current = current_raw * 0.0005f;  // LSB = 0.5mA

        memcpy(can_data, &voltage, sizeof(float));
        send_sensor_data(can_dev, CAN_ID_BATTERY, can_data, BATTERY_MSG_LEN);

        k_sleep(K_MSEC(1000));
    }
}

void main(void) {
    i2c_dev = DEVICE_DT_GET(DT_ALIAS(i2c_sens));
    can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

    if (!device_is_ready(i2c_dev) || !device_is_ready(can_dev)) {
        return;
    }

    // Configure INA219
    uint16_t config = 0x399F; // 32V, ±2A range
    uint8_t config_data[2] = {config >> 8, config & 0xFF};
    i2c_write(i2c_dev, config_data, 2, INA219_ADDR);

    k_thread_create(&battery_thread_data, battery_stack,
                   SENSOR_THREAD_STACK_SIZE,
                   battery_thread, NULL, NULL, NULL,
                   SENSOR_THREAD_PRIORITY, 0, K_NO_WAIT);
}
