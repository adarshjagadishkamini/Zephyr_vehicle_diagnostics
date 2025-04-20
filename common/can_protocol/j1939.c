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

// Transport Protocol (TP) support
static void handle_tp_cm(struct j1939_ctx *ctx, const struct can_frame *frame) {
    uint8_t tp_cmd = frame->data[0];
    
    switch (tp_cmd) {
        case TP_CM_RTS:
            handle_tp_rts(ctx, frame);
            break;
        case TP_CM_CTS:
            handle_tp_cts(ctx, frame);
            break;
        case TP_CM_EndOfMsgAck:
            handle_tp_eom_ack(ctx, frame);
            break;
        case TP_CM_BAM:
            handle_tp_bam(ctx, frame);
            break;
    }
}

void j1939_process_message(struct j1939_ctx *ctx, struct can_frame *frame) {
    uint32_t pgn = (frame->id >> 8) & 0x1FFFF;
    
    if (pgn == PGN_TP_CM) {
        handle_tp_cm(ctx, frame);
        return;
    }
    
    // Handle standard PGNs
    switch (pgn) {
        case J1939_PGN_ENGINE_TEMP:
            process_engine_temp(ctx, frame);
            break;
        case J1939_PGN_VEHICLE_SPEED:
            process_vehicle_speed(ctx, frame);
            break;
        // Add more PGN handlers
    }
}
