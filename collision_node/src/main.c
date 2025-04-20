#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/can.h>
#include "sensor_common.h"
#include "can_ids.h"

#define TRIG_PIN 13
#define ECHO_PIN 14

static const struct device *gpio_dev;
static const struct device *can_dev;

static uint32_t measure_distance(void)
{
    uint32_t start_time, stop_time;
    
    // Send trigger pulse
    gpio_pin_set(gpio_dev, TRIG_PIN, 1);
    k_busy_wait(10);  // 10us pulse
    gpio_pin_set(gpio_dev, TRIG_PIN, 0);
    
    // Wait for echo
    while (gpio_pin_get(gpio_dev, ECHO_PIN) == 0) {
        if (k_cycle_get_32() - start_time > k_ms_to_cyc_floor32(30)) {
            return 0; // Timeout
        }
    }
    start_time = k_cycle_get_32();
    
    while (gpio_pin_get(gpio_dev, ECHO_PIN) == 1) {
        if (k_cycle_get_32() - start_time > k_ms_to_cyc_floor32(30)) {
            return 0; // Timeout
        }
    }
    stop_time = k_cycle_get_32();
    
    // Calculate distance in cm (speed of sound = 343m/s)
    return (stop_time - start_time) * 343 / (2 * sys_clock_hw_cycles_per_sec());
}

static void collision_thread(void *arg1, void *arg2, void *arg3)
{
    uint8_t can_data[COLLISION_MSG_LEN];
    uint16_t distance;

    while (1) {
        distance = measure_distance();
        can_data[0] = distance >> 8;
        can_data[1] = distance & 0xFF;
        
        send_sensor_data(can_dev, CAN_ID_COLLISION, can_data, COLLISION_MSG_LEN);
        
        k_sleep(K_MSEC(100));  // 10Hz sampling rate
    }
}

void main(void) {
    gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

    if (!device_is_ready(gpio_dev) || !device_is_ready(can_dev)) {
        return;
    }

    gpio_pin_configure(gpio_dev, TRIG_PIN, GPIO_OUTPUT);
    gpio_pin_configure(gpio_dev, ECHO_PIN, GPIO_INPUT);

    k_thread_create(&collision_thread_data, collision_stack,
                   SENSOR_THREAD_STACK_SIZE,
                   collision_thread, NULL, NULL, NULL,
                   SENSOR_THREAD_PRIORITY, 0, K_NO_WAIT);
}
