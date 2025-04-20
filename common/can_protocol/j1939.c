#include "j1939.h"

#define J1939_PF_MASK    0xFF0000
#define J1939_PS_MASK    0x0000FF
#define J1939_DP_MASK    0x010000

int j1939_init(struct j1939_ctx *ctx) {
    if (!device_is_ready(ctx->can_dev)) {
        return -ENODEV;
    }
    
    // Set up J1939 specific CAN filters
    struct can_filter filter = {
        .id = ctx->source_address,
        .mask = 0xFF,
        .flags = CAN_FILTER_IDE
    };
    
    return can_add_rx_filter(ctx->can_dev, j1939_message_handler, ctx, &filter);
}

static uint32_t build_j1939_id(uint32_t pgn, uint8_t priority, uint8_t sa, uint8_t da) {
    return ((uint32_t)priority << 26) | (pgn << 8) | sa;
}

int j1939_send_pgn(struct j1939_ctx *ctx, uint32_t pgn, const uint8_t *data, uint8_t len) {
    struct can_frame frame = {
        .id = build_j1939_id(pgn, 6, ctx->source_address, ctx->dest_address),
        .dlc = len,
        .flags = CAN_FRAME_IDE
    };
    
    memcpy(frame.data, data, len);
    return can_send(ctx->can_dev, &frame, K_MSEC(100), NULL, NULL);
}
