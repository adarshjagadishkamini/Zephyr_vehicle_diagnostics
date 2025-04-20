#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/can.h>
#include "sensor_common.h"
#include "can_ids.h"

#define GPS_UART_NODE DT_ALIAS(uart_gps)
#define GPS_BUFFER_SIZE 256

static const struct device *uart_dev;
static const struct device *can_dev;
static uint8_t rx_buf[GPS_BUFFER_SIZE];

static void uart_cb(const struct device *dev, void *user_data)
{
    uint8_t c;
    static uint8_t pos = 0;

    if (uart_poll_in(dev, &c) == 0) {
        if (c == '\n') {
            if (pos > 6 && strncmp((char *)rx_buf, "$GPRMC", 6) == 0) {
                // Parse NMEA data and send over CAN
                float lat, lon;
                parse_gps_data(rx_buf, pos, &lat, &lon);
                
                uint8_t can_data[GPS_MSG_LEN];
                memcpy(can_data, &lat, sizeof(float));
                memcpy(can_data + sizeof(float), &lon, sizeof(float));
                send_sensor_data(can_dev, CAN_ID_GPS, can_data, GPS_MSG_LEN);
            }
            pos = 0;
        } else if (pos < GPS_BUFFER_SIZE - 1) {
            rx_buf[pos++] = c;
        }
    }
}

void main(void) {
    uart_dev = DEVICE_DT_GET(GPS_UART_NODE);
    can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

    if (!device_is_ready(uart_dev) || !device_is_ready(can_dev)) {
        return;
    }

    uart_irq_callback_set(uart_dev, uart_cb);
    uart_irq_rx_enable(uart_dev);
}
