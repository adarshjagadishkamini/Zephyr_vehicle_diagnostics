#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include "sensor_common.h"
#include "can_ids.h"

#define DS18B20_PIN 5  // GPIO pin for 1-Wire

static const struct device *temp_dev;
static const struct device *can_dev;

void temperature_thread(void *arg1, void *arg2, void *arg3) {
    struct sensor_value temp;
    uint8_t data[TEMP_MSG_LEN];
    float temperature;

    while (1) {
        // Read temperature from DS18B20
        if (sensor_sample_fetch(temp_dev) == 0) {
            sensor_channel_get(temp_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
            
            // Convert to float for CAN transmission
            temperature = sensor_value_to_double(&temp);
            memcpy(data, &temperature, sizeof(float));

            // Send over CAN
            send_sensor_data(can_dev, CAN_ID_TEMP, data, TEMP_MSG_LEN);
        }
        
        k_sleep(K_MSEC(1000));
    }
}

void main(void) {
    // Initialize CAN
    can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
    if (!device_is_ready(can_dev)) {
        return;
    }

    // Initialize temperature sensor
    temp_dev = DEVICE_DT_GET_ONE(maxim_ds18b20);
    if (!device_is_ready(temp_dev)) {
        return;
    }

    // Create temperature monitoring thread
    k_thread_create(&temp_thread_data, temp_thread_stack,
                   SENSOR_THREAD_STACK_SIZE,
                   temperature_thread, NULL, NULL, NULL,
                   SENSOR_THREAD_PRIORITY, 0, K_NO_WAIT);
}
