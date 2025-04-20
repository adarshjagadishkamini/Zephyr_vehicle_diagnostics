#ifndef J1939_H
#define J1939_H

#include <zephyr/drivers/can.h>

// J1939 PGNs
#define J1939_PGN_ENGINE_TEMP    0xFEE6
#define J1939_PGN_VEHICLE_SPEED  0xFEF1
#define J1939_PGN_BRAKE_INFO     0xFE4E

struct j1939_ctx {
    const struct device *can_dev;
    uint8_t source_address;
    uint8_t dest_address;
};

int j1939_init(struct j1939_ctx *ctx);
int j1939_send_pgn(struct j1939_ctx *ctx, uint32_t pgn, const uint8_t *data, uint8_t len);
void j1939_process_message(struct j1939_ctx *ctx, struct can_frame *frame);

#endif /* J1939_H */
