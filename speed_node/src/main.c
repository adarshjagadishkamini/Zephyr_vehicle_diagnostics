#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/can.h>
#include "sensor_common.h"
#include "can_ids.h"

#define HALL_SENSOR_PIN 5
#define WHEEL_CIRCUMFERENCE 2.0f  // meters
#define PULSES_PER_REVOLUTION 4

static const struct device *gpio_dev;
static const struct device *can_dev;
static volatile uint32_t pulse_count = 0;
static volatile uint64_t last_pulse_time = 0;

static void hall_sensor_callback(const struct device *dev, 
                               struct gpio_callback *cb, uint32_t pins) {
    uint64_t current_time = k_uptime_get();
    pulse_count++;
    last_pulse_time = current_time;
}

static void speed_thread(void *arg1, void *arg2, void *arg3) {
    uint8_t can_data[SPEED_MSG_LEN];
    uint16_t speed_kmh;
    uint32_t last_count = 0;
    uint64_t last_time = k_uptime_get();

    while (1) {
        uint64_t current_time = k_uptime_get();
        uint32_t current_count = pulse_count;
        uint32_t pulses = current_count - last_count;
        float time_diff = (current_time - last_time) / 1000.0f;

        if (time_diff > 0) {
            // Calculate speed in km/h
            float rotations = (float)pulses / PULSES_PER_REVOLUTION;
            float distance = rotations * WHEEL_CIRCUMFERENCE;
            float speed = (distance / time_diff) * 3.6f;  // Convert m/s to km/h
            
            speed_kmh = (uint16_t)speed;
            can_data[0] = speed_kmh >> 8;
            can_data[1] = speed_kmh & 0xFF;
            
            send_sensor_data(can_dev, CAN_ID_SPEED, can_data, SPEED_MSG_LEN);
        }

        last_count = current_count;
        last_time = current_time;
        k_sleep(K_MSEC(100));  // 10Hz update rate
    }
}

void main(void) {
    gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

    if (!device_is_ready(gpio_dev) || !device_is_ready(can_dev)) {
        return;
    }

    // Configure Hall effect sensor GPIO
    gpio_pin_configure(gpio_dev, HALL_SENSOR_PIN, GPIO_INPUT | GPIO_INT_EDGE_RISING);
    
    static struct gpio_callback hall_cb;
    gpio_init_callback(&hall_cb, hall_sensor_callback, BIT(HALL_SENSOR_PIN));
    gpio_add_callback(gpio_dev, &hall_cb);
    gpio_pin_interrupt_configure(gpio_dev, HALL_SENSOR_PIN, GPIO_INT_EDGE_RISING);

    k_thread_create(&speed_thread_data, speed_stack,
                   SENSOR_THREAD_STACK_SIZE,
                   speed_thread, NULL, NULL, NULL,
                   SENSOR_THREAD_PRIORITY, 0, K_NO_WAIT);
}
