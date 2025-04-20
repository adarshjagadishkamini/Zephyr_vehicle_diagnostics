#ifndef ISOTP_H
#define ISOTP_H

#include <zephyr/drivers/can.h>

// ISO-TP frame types
#define ISOTP_SINGLE_FRAME    0x00
#define ISOTP_FIRST_FRAME     0x01
#define ISOTP_CONSECUTIVE     0x02
#define ISOTP_FLOW_CONTROL    0x03

struct isotp_ctx {
    const struct device *can_dev;
    uint32_t rx_id;
    uint32_t tx_id;
    uint8_t *buf;
    size_t buf_size;
};

int isotp_init(struct isotp_ctx *ctx);
int isotp_send(struct isotp_ctx *ctx, const uint8_t *data, size_t len);
int isotp_receive(struct isotp_ctx *ctx, uint8_t *data, size_t len);

#endif /* ISOTP_H */
