#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include "sensor_common.h"
#include "can_ids.h"

#define SP370_CS_PIN  2
#define SP370_SDO_PIN 3
#define SP370_CLK_PIN 4

static const struct device *spi_dev;
static const struct device *can_dev;

static void tpms_thread(void *arg1, void *arg2, void *arg3) {
    uint8_t pressure_data;
    struct spi_buf rx_buf = { .buf = &pressure_data, .len = 1 };
    struct spi_buf_set rx = { .buffers = &rx_buf, .count = 1 };

    while (1) {
        // Read pressure from SP370 sensor
        if (spi_read(spi_dev, &rx) == 0) {
            // Send pressure data over CAN
            send_sensor_data(can_dev, CAN_ID_TPMS, &pressure_data, TPMS_MSG_LEN);
        }
        k_sleep(K_MSEC(1000));
    }
}

void main(void) {
    // Initialize devices
    spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi0));
    can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

    if (!device_is_ready(spi_dev) || !device_is_ready(can_dev)) {
        return;
    }

    k_thread_create(&tpms_thread_data, tpms_stack,
                   SENSOR_THREAD_STACK_SIZE,
                   tpms_thread, NULL, NULL, NULL,
                   SENSOR_THREAD_PRIORITY, 0, K_NO_WAIT);
}
