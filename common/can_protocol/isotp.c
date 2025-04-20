#include "isotp.h"

#define ISO_TP_TIMEOUT_MS 1000
#define ISO_TP_BLOCK_SIZE 8
#define ISO_TP_STM 10

struct isotp_tx_ctx {
    uint8_t seq_id;
    const uint8_t *data;
    size_t remaining;
    size_t total_len;
};

int isotp_init(struct isotp_ctx *ctx) {
    if (!device_is_ready(ctx->can_dev)) {
        return -ENODEV;
    }
    return 0;
}

static void isotp_send_consecutive(const struct device *dev, struct isotp_tx_ctx *tx_ctx, uint32_t tx_id) {
    struct can_frame frame = {
        .id = tx_id,
        .dlc = 8,
    };
    
    frame.data[0] = ISOTP_CONSECUTIVE | tx_ctx->seq_id;
    size_t len = MIN(7, tx_ctx->remaining);
    memcpy(&frame.data[1], tx_ctx->data, len);
    
    can_send(dev, &frame, ISO_TP_TIMEOUT_MS, NULL, NULL);
    tx_ctx->data += len;
    tx_ctx->remaining -= len;
    tx_ctx->seq_id = (tx_ctx->seq_id + 1) & 0x0F;
}

int isotp_send(struct isotp_ctx *ctx, const uint8_t *data, size_t len) {
    struct isotp_tx_ctx tx_ctx = {
        .seq_id = 0,
        .data = data,
        .remaining = len,
        .total_len = len
    };

    if (len <= 7) {
        // Single frame
        struct can_frame frame = {
            .id = ctx->tx_id,
            .dlc = len + 1,
            .data[0] = ISOTP_SINGLE_FRAME | len
        };
        memcpy(&frame.data[1], data, len);
        return can_send(ctx->can_dev, &frame, ISO_TP_TIMEOUT_MS, NULL, NULL);
    } else {
        // Multi frame transmission
        struct can_frame frame = {
            .id = ctx->tx_id,
            .dlc = 8,
            .data[0] = ISOTP_FIRST_FRAME | ((len >> 8) & 0x0F),
            .data[1] = len & 0xFF
        };
        memcpy(&frame.data[2], data, 6);
        can_send(ctx->can_dev, &frame, ISO_TP_TIMEOUT_MS, NULL, NULL);
        
        tx_ctx.data += 6;
        tx_ctx.remaining -= 6;
        
        while (tx_ctx.remaining > 0) {
            isotp_send_consecutive(ctx->can_dev, &tx_ctx, ctx->tx_id);
            k_sleep(K_MSEC(ISO_TP_STM));
        }
        return 0;
    }
}
