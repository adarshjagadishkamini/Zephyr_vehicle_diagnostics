#include <zephyr/kernel.h>
#include <string.h>
#include "j1939.h"

#define J1939_PF_MASK    0xFF0000
#define J1939_PS_MASK    0x0000FF
#define J1939_DP_MASK    0x010000
#define MAX_PGN_HANDLERS 32
#define TP_MAX_PACKETS   255
#define TP_DATA_SIZE     7
#define TP_TIMEOUT_MS    750

struct pgn_handler {
    uint32_t pgn;
    void (*handler)(struct j1939_ctx *, const uint8_t *, uint16_t);
};

static struct pgn_handler pgn_handlers[MAX_PGN_HANDLERS];
static uint8_t num_handlers = 0;

// Transport Protocol buffers
static uint8_t tp_buffer[1785]; // Max J1939-TP size
static K_SEM_DEFINE(tp_sem, 0, 1);

static uint32_t build_j1939_id(uint32_t pgn, uint8_t priority, uint8_t sa, uint8_t da) {
    return ((uint32_t)priority << 26) | (pgn << 8) | sa;
}

int j1939_init(struct j1939_ctx *ctx) {
    if (!device_is_ready(ctx->can_dev)) {
        return -ENODEV;
    }
    
    ctx->tp_in_progress = false;
    memset(pgn_handlers, 0, sizeof(pgn_handlers));
    
    // Set up J1939 specific CAN filters
    struct can_filter filter = {
        .id = ctx->source_address,
        .mask = 0xFF,
        .flags = CAN_FILTER_IDE
    };
    
    return can_add_rx_filter(ctx->can_dev, NULL, ctx, &filter);
}

int j1939_register_pgn_handler(uint32_t pgn, void (*handler)(struct j1939_ctx *, const uint8_t *, uint16_t)) {
    if (num_handlers >= MAX_PGN_HANDLERS) {
        return -ENOMEM;
    }
    
    pgn_handlers[num_handlers].pgn = pgn;
    pgn_handlers[num_handlers].handler = handler;
    num_handlers++;
    
    return 0;
}

static void handle_tp_rts(struct j1939_ctx *ctx, const struct can_frame *frame) {
    if (ctx->tp_in_progress) {
        // Already in a session, send abort
        uint8_t abort_msg[8] = {TP_CM_Abort, 0x01, 0xFF, 0xFF};
        j1939_send_pgn(ctx, J1939_PGN_TP_CM, abort_msg, 8);
        return;
    }
    
    uint16_t size = (frame->data[1] | (frame->data[2] << 8));
    uint8_t num_packets = frame->data[3];
    uint32_t pgn = (frame->data[5] | (frame->data[6] << 8) | (frame->data[7] << 16));
    
    // Validate size and packet count
    if (size > sizeof(tp_buffer) || size == 0 || 
        num_packets == 0 || num_packets > TP_MAX_PACKETS ||
        size != ((num_packets - 1) * 7 + (size % 7 ? size % 7 : 7))) {
        uint8_t abort_msg[8] = {TP_CM_Abort, 0x02, 0xFF, 0xFF};
        j1939_send_pgn(ctx, J1939_PGN_TP_CM, abort_msg, 8);
        return;
    }
    
    // Start new TP session
    ctx->tp_session.pgn = pgn;
    ctx->tp_session.total_size = size;
    ctx->tp_session.num_packets = num_packets;
    ctx->tp_session.next_packet = 1;
    ctx->tp_session.data = tp_buffer;
    ctx->tp_session.start_time = k_uptime_get_32();
    ctx->tp_in_progress = true;
    
    // Send CTS
    uint8_t cts_msg[8] = {TP_CM_CTS, num_packets, 1, 0xFF, 0xFF};
    cts_msg[5] = pgn & 0xFF;
    cts_msg[6] = (pgn >> 8) & 0xFF;
    cts_msg[7] = (pgn >> 16) & 0xFF;
    
    j1939_send_pgn(ctx, J1939_PGN_TP_CM, cts_msg, 8);
}

static void handle_tp_dt(struct j1939_ctx *ctx, const struct can_frame *frame) {
    if (!ctx->tp_in_progress) {
        return;
    }
    
    // Check session timeout
    if (k_uptime_get_32() - ctx->tp_session.start_time > TP_TIMEOUT_MS) {
        uint8_t abort_msg[8] = {TP_CM_Abort, 0x03, 0xFF, 0xFF};
        j1939_send_pgn(ctx, J1939_PGN_TP_CM, abort_msg, 8);
        ctx->tp_in_progress = false;
        return;
    }
    
    uint8_t seq = frame->data[0];
    if (seq != ctx->tp_session.next_packet) {
        // Sequence error
        uint8_t abort_msg[8] = {TP_CM_Abort, 0x04, 0xFF, 0xFF};
        j1939_send_pgn(ctx, J1939_PGN_TP_CM, abort_msg, 8);
        ctx->tp_in_progress = false;
        return;
    }
    
    uint16_t offset = (seq - 1) * 7;
    uint8_t len = MIN(7, ctx->tp_session.total_size - offset);
    
    // Validate offset and length
    if (offset + len > ctx->tp_session.total_size) {
        uint8_t abort_msg[8] = {TP_CM_Abort, 0x05, 0xFF, 0xFF};
        j1939_send_pgn(ctx, J1939_PGN_TP_CM, abort_msg, 8);
        ctx->tp_in_progress = false;
        return;
    }
    
    memcpy(ctx->tp_session.data + offset, &frame->data[1], len);
    ctx->tp_session.next_packet++;
    
    // Check if transfer complete
    if (ctx->tp_session.next_packet > ctx->tp_session.num_packets) {
        // Validate total size
        if (ctx->tp_session.total_size != offset + len) {
            uint8_t abort_msg[8] = {TP_CM_Abort, 0x06, 0xFF, 0xFF};
            j1939_send_pgn(ctx, J1939_PGN_TP_CM, abort_msg, 8);
            ctx->tp_in_progress = false;
            return;
        }
        
        // Send End of Message ACK
        uint8_t eom_msg[8] = {TP_CM_EndOfMsgAck, 
                             ctx->tp_session.total_size & 0xFF,
                             (ctx->tp_session.total_size >> 8) & 0xFF,
                             ctx->tp_session.num_packets,
                             0xFF};
        eom_msg[5] = ctx->tp_session.pgn & 0xFF;
        eom_msg[6] = (ctx->tp_session.pgn >> 8) & 0xFF;
        eom_msg[7] = (ctx->tp_session.pgn >> 16) & 0xFF;
        j1939_send_pgn(ctx, J1939_PGN_TP_CM, eom_msg, 8);
        
        // Process complete message
        for (int i = 0; i < num_handlers; i++) {
            if (pgn_handlers[i].pgn == ctx->tp_session.pgn) {
                pgn_handlers[i].handler(ctx, ctx->tp_session.data, 
                                      ctx->tp_session.total_size);
                break;
            }
        }
        
        ctx->tp_in_progress = false;
        k_sem_give(&tp_sem);
    }
}

int j1939_send_tp_data(struct j1939_ctx *ctx, uint32_t pgn, const uint8_t *data, uint16_t len) {
    if (len <= 8) {
        return j1939_send_pgn(ctx, pgn, data, len);
    }
    
    uint8_t num_packets = (len + 6) / 7;
    
    // Send RTS
    uint8_t rts_msg[8] = {TP_CM_RTS, len & 0xFF, (len >> 8) & 0xFF, num_packets, 0xFF};
    rts_msg[5] = pgn & 0xFF;
    rts_msg[6] = (pgn >> 8) & 0xFF;
    rts_msg[7] = (pgn >> 16) & 0xFF;
    
    int ret = j1939_send_pgn(ctx, J1939_PGN_TP_CM, rts_msg, 8);
    if (ret < 0) {
        return ret;
    }
    
    // Send data packets
    for (uint8_t i = 0; i < num_packets; i++) {
        uint8_t packet[8];
        packet[0] = i + 1;
        uint8_t bytes = MIN(7, len - (i * 7));
        memcpy(&packet[1], data + (i * 7), bytes);
        
        ret = j1939_send_pgn(ctx, J1939_PGN_TP_DT, packet, 8);
        if (ret < 0) {
            return ret;
        }
        k_sleep(K_MSEC(50)); // Spacing between packets
    }
    
    return 0;
}

int j1939_send_pgn(struct j1939_ctx *ctx, uint32_t pgn, const uint8_t *data, uint8_t len) {
    struct can_frame frame = {
        .id = build_j1939_id(pgn, J1939_PRIORITY_MEDIUM, ctx->source_address, ctx->dest_address),
        .dlc = len,
        .flags = CAN_FRAME_IDE
    };
    
    memcpy(frame.data, data, len);
    return can_send(ctx->can_dev, &frame, K_MSEC(100), NULL, NULL);
}

void j1939_process_message(struct j1939_ctx *ctx, struct can_frame *frame) {
    uint32_t pgn = (frame->id >> 8) & 0x1FFFF;
    
    // Handle Transport Protocol messages
    if (pgn == J1939_PGN_TP_CM) {
        uint8_t tp_cmd = frame->data[0];
        switch (tp_cmd) {
            case TP_CM_RTS:
                handle_tp_rts(ctx, frame);
                break;
            case TP_CM_CTS:
                // Implement if needed for multi-packet send
                break;
            case TP_CM_EndOfMsgAck:
                ctx->tp_in_progress = false;
                k_sem_give(&tp_sem);
                break;
            case TP_CM_BAM:
                // Implement Broadcast Announce Message handling if needed
                break;
            case TP_CM_Abort:
                ctx->tp_in_progress = false;
                k_sem_give(&tp_sem);
                break;
        }
        return;
    }
    
    if (pgn == J1939_PGN_TP_DT) {
        handle_tp_dt(ctx, frame);
        return;
    }
    
    // Handle standard PGNs
    for (int i = 0; i < num_handlers; i++) {
        if (pgn_handlers[i].pgn == pgn) {
            pgn_handlers[i].handler(ctx, frame->data, frame->dlc);
            break;
        }
    }
}
