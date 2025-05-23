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

int isotp_receive(struct isotp_ctx *ctx, uint8_t *dest, size_t len) {
    struct can_frame frame;
    size_t remaining = len;
    uint32_t start_time = k_uptime_get_32();
    
    // Receive first frame with timeout validation
    if (can_receive(ctx->can_dev, &frame, K_MSEC(ISO_TP_TIMEOUT_MS)) != 0) {
        return -ETIMEDOUT;
    }
    
    if ((frame.id & ctx->rx_id) != ctx->rx_id) {
        return -EINVAL;  // Invalid ID
    }
    
    uint8_t frame_type = frame.data[0] & 0xF0;
    if (frame_type == ISOTP_SINGLE_FRAME) {
        size_t recv_len = frame.data[0] & 0x0F;
        if (recv_len == 0 || recv_len > 7) {
            return -EMSGSIZE;  // Invalid length
        }
        if (recv_len > len) {
            return -ENOSPC;  // Buffer too small
        }
        memcpy(dest, &frame.data[1], recv_len);
        return recv_len;
    } 
    
    if (frame_type == ISOTP_FIRST_FRAME) {
        size_t total_len = ((frame.data[0] & 0x0F) << 8) | frame.data[1];
        if (total_len > len || total_len < 8) {
            return -EMSGSIZE;  // Invalid length
        }
        
        // Copy initial data
        memcpy(dest, &frame.data[2], 6);
        dest += 6;
        remaining = total_len - 6;
        
        // Send flow control with validation
        struct can_frame fc = {
            .id = ctx->tx_id,
            .dlc = 3,
            .data = {ISOTP_FLOW_CONTROL, ISO_TP_BLOCK_SIZE, ISO_TP_STM}
        };
        
        int ret = can_send(ctx->can_dev, &fc, K_MSEC(100), NULL, NULL);
        if (ret != 0) {
            return ret;  // Flow control send failed
        }
        
        // Receive consecutive frames with sequence validation
        uint8_t expected_seq = 1;
        while (remaining > 0) {
            // Check overall timeout
            if (k_uptime_get_32() - start_time > ISO_TP_TIMEOUT_MS) {
                return -ETIMEDOUT;
            }
            
            if (can_receive(ctx->can_dev, &frame, K_MSEC(ISO_TP_TIMEOUT_MS)) != 0) {
                return -ETIMEDOUT;
            }
            
            if ((frame.data[0] & 0xF0) != ISOTP_CONSECUTIVE) {
                return -EINVAL;  // Invalid frame type
            }
            
            uint8_t seq = frame.data[0] & 0x0F;
            if (seq != expected_seq) {
                return -EINVAL;  // Sequence mismatch
            }
            
            size_t copy_len = MIN(7, remaining);
            memcpy(dest, &frame.data[1], copy_len);
            dest += copy_len;
            remaining -= copy_len;
            
            expected_seq = (expected_seq + 1) & 0x0F;
            
            // Optional: Add flow control for large transfers
            if (ISO_TP_BLOCK_SIZE > 0 && 
                (expected_seq % ISO_TP_BLOCK_SIZE) == 0 && 
                remaining > 0) {
                fc.data[1] = MIN(ISO_TP_BLOCK_SIZE, (remaining + 6) / 7);
                ret = can_send(ctx->can_dev, &fc, K_MSEC(100), NULL, NULL);
                if (ret != 0) {
                    return ret;
                }
            }
        }
        
        return total_len;
    }
    
    return -EINVAL;  // Unknown frame type
}
