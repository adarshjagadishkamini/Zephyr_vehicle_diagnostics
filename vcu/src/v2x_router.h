#ifndef V2X_ROUTER_H
#define V2X_ROUTER_H

#include <zephyr/kernel.h>
#include <zephyr/crypto/crypto.h>

// Message types
#define V2X_MSG_EMERGENCY    0x01
#define V2X_MSG_TRAFFIC      0x02
#define V2X_MSG_WEATHER      0x03
#define V2X_MSG_ROAD_HAZARD  0x04

// Message validation flags
#define V2X_FLAG_AUTHENTICATED  0x01
#define V2X_FLAG_ENCRYPTED     0x02
#define V2X_FLAG_TIMESTAMPED   0x04
#define V2X_FLAG_SIGNED        0x08

// Validation thresholds
#define V2X_MAX_AGE_MS        5000
#define V2X_MIN_PRIORITY      0
#define V2X_MAX_PRIORITY      7
#define V2X_MAX_DATA_SIZE     256
#define V2X_HMAC_SIZE         32

// Error codes
#define V2X_ERR_INVALID_TYPE     -1
#define V2X_ERR_INVALID_SIZE     -2
#define V2X_ERR_INVALID_AUTH     -3
#define V2X_ERR_EXPIRED          -4
#define V2X_ERR_NO_HANDLER       -5
#define V2X_ERR_INVALID_PRIORITY -6

struct v2x_message {
    uint8_t msg_type;
    uint8_t priority;
    uint32_t source_id;
    uint32_t timestamp;
    uint8_t flags;
    uint8_t data[V2X_MAX_DATA_SIZE];
    uint16_t data_len;
    uint8_t hmac[V2X_HMAC_SIZE];
};

// Message validation config
struct v2x_validation_config {
    bool require_auth;
    bool require_encryption;
    bool require_timestamp;
    uint32_t max_age_ms;
    uint8_t min_priority;
};

// Function prototypes
void v2x_router_init(void);
int v2x_route_message(struct v2x_message *msg);
void register_v2x_handler(uint8_t msg_type, void (*handler)(struct v2x_message *));
int v2x_validate_message(const struct v2x_message *msg, const struct v2x_validation_config *config);
void v2x_set_validation_config(const struct v2x_validation_config *config);
int v2x_authenticate_message(struct v2x_message *msg);
bool v2x_verify_timestamp(uint32_t timestamp, uint32_t max_age_ms);
const char *v2x_strerror(int error_code);

#endif /* V2X_ROUTER_H */
