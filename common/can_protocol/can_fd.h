#ifndef CAN_FD_H
#define CAN_FD_H

#include <zephyr/drivers/can.h>

#define CAN_FD_MAX_DLC 64

struct can_fd_frame {
    uint32_t id;
    uint8_t flags;
    uint8_t len;
    uint8_t data[CAN_FD_MAX_DLC];
};

int can_fd_init(const struct device *dev);
int can_fd_send(const struct device *dev, const struct can_fd_frame *frame);
int can_fd_add_rx_filter(const struct device *dev, can_rx_callback_t callback);

#endif /* CAN_FD_H */
