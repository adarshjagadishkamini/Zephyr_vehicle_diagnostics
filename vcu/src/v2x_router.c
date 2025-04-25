#include <zephyr/kernel.h>
#include <zephyr/crypto/crypto.h>
#include <string.h>
#include "v2x_router.h"
#include "can_auth.h"
#include "error_handler.h"

#define MAX_HANDLERS 16
#define CRYPTO_BLOCK_SIZE 16
#define MAX_VALIDATION_RETRIES 3

struct v2x_handler {
    uint8_t msg_type;
    void (*handler)(struct v2x_message *);
};

static struct v2x_handler handlers[MAX_HANDLERS];
static uint8_t num_handlers = 0;
static struct v2x_validation_config validation_config;
static struct device *crypto_dev;

// Symmetric key for HMAC (in practice, use secure storage)
static uint8_t hmac_key[32] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};

void v2x_router_init(void) {
    memset(handlers, 0, sizeof(handlers));
    
    // Set default validation config
    validation_config.require_auth = true;
    validation_config.require_encryption = false;
    validation_config.require_timestamp = true;
    validation_config.max_age_ms = V2X_MAX_AGE_MS;
    validation_config.min_priority = V2X_MIN_PRIORITY;
    
    // Initialize crypto device
    crypto_dev = device_get_binding(CONFIG_CRYPTO_DEV_NAME);
    if (!crypto_dev) {
        handle_error(ERROR_CRYPTO_INIT_FAILED);
    }
}

void v2x_set_validation_config(const struct v2x_validation_config *config) {
    memcpy(&validation_config, config, sizeof(struct v2x_validation_config));
}

int v2x_validate_message(const struct v2x_message *msg, const struct v2x_validation_config *config) {
    if (!msg || !config) {
        return V2X_ERR_INVALID_TYPE;
    }
    
    // Validate message type
    if (msg->msg_type < V2X_MSG_EMERGENCY || msg->msg_type > V2X_MSG_ROAD_HAZARD) {
        return V2X_ERR_INVALID_TYPE;
    }
    
    // Validate size
    if (msg->data_len > V2X_MAX_DATA_SIZE) {
        return V2X_ERR_INVALID_SIZE;
    }
    
    // Validate priority
    if (msg->priority > V2X_MAX_PRIORITY || msg->priority < config->min_priority) {
        return V2X_ERR_INVALID_PRIORITY;
    }
    
    // Check timestamp if required
    if (config->require_timestamp) {
        if (!(msg->flags & V2X_FLAG_TIMESTAMPED) || 
            !v2x_verify_timestamp(msg->timestamp, config->max_age_ms)) {
            return V2X_ERR_EXPIRED;
        }
    }
    
    // Verify authentication if required
    if (config->require_auth && !(msg->flags & V2X_FLAG_AUTHENTICATED)) {
        return V2X_ERR_INVALID_AUTH;
    }
    
    return 0;
}

bool v2x_verify_timestamp(uint32_t timestamp, uint32_t max_age_ms) {
    uint32_t current_time = k_uptime_get_32();
    uint32_t age = current_time - timestamp;
    return age <= max_age_ms;
}

int v2x_authenticate_message(struct v2x_message *msg) {
    struct cipher_ctx ctx;
    struct cipher_pkt pkt;
    uint8_t computed_hmac[V2X_HMAC_SIZE];
    
    // Set up HMAC computation
    pkt.in_buf = (uint8_t *)msg;
    pkt.in_len = offsetof(struct v2x_message, hmac);
    pkt.out_buf = computed_hmac;
    pkt.out_buf_max = sizeof(computed_hmac);
    
    // Configure HMAC operation
    struct cipher_ops opts = {
        .mode = CRYPTO_CIPHER_MODE_HMAC,
        .algo = CRYPTO_CIPHER_ALGO_SHA256
    };
    
    int ret = cipher_begin_session(crypto_dev, &ctx, &opts);
    if (ret) {
        return ret;
    }
    
    ret = cipher_hash_finalize(&ctx, &pkt);
    cipher_free_session(crypto_dev, &ctx);
    
    if (ret) {
        return ret;
    }
    
    // Compare computed HMAC with message HMAC
    if (memcmp(computed_hmac, msg->hmac, V2X_HMAC_SIZE) != 0) {
        return V2X_ERR_INVALID_AUTH;
    }
    
    msg->flags |= V2X_FLAG_AUTHENTICATED;
    return 0;
}

int v2x_route_message(struct v2x_message *msg) {
    int ret;
    uint8_t retries = 0;
    
    // Validate message
    ret = v2x_validate_message(msg, &validation_config);
    if (ret != 0) {
        return ret;
    }
    
    // Authenticate if needed
    if (validation_config.require_auth && !(msg->flags & V2X_FLAG_AUTHENTICATED)) {
        while (retries < MAX_VALIDATION_RETRIES) {
            ret = v2x_authenticate_message(msg);
            if (ret == 0) {
                break;
            }
            retries++;
        }
        if (ret != 0) {
            return ret;
        }
    }
    
    // Route to appropriate handler
    for (int i = 0; i < num_handlers; i++) {
        if (handlers[i].msg_type == msg->msg_type) {
            handlers[i].handler(msg);
            return 0;
        }
    }
    
    return V2X_ERR_NO_HANDLER;
}

void register_v2x_handler(uint8_t msg_type, void (*handler)(struct v2x_message *)) {
    if (num_handlers < MAX_HANDLERS && handler != NULL) {
        handlers[num_handlers].msg_type = msg_type;
        handlers[num_handlers].handler = handler;
        num_handlers++;
    }
}

const char *v2x_strerror(int error_code) {
    switch (error_code) {
        case 0:
            return "Success";
        case V2X_ERR_INVALID_TYPE:
            return "Invalid message type";
        case V2X_ERR_INVALID_SIZE:
            return "Invalid message size";
        case V2X_ERR_INVALID_AUTH:
            return "Authentication failed";
        case V2X_ERR_EXPIRED:
            return "Message expired";
        case V2X_ERR_NO_HANDLER:
            return "No handler registered";
        case V2X_ERR_INVALID_PRIORITY:
            return "Invalid message priority";
        default:
            return "Unknown error";
    }
}
