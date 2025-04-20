#include "emergency_handler.h"
#include "v2x_router.h"

#define MAX_EMERGENCY_EVENTS 16

struct emergency_event {
    uint32_t id;
    uint8_t type;
    uint8_t severity;
    float latitude;
    float longitude;
    uint32_t timestamp;
};

static struct emergency_event events[MAX_EMERGENCY_EVENTS];
static uint8_t num_events = 0;

void emergency_handler_init(void) {
    memset(events, 0, sizeof(events));
    register_v2x_handler(V2X_MSG_EMERGENCY, handle_emergency_message);
}

void broadcast_emergency_alert(struct emergency_event *event) {
    struct v2x_message msg = {
        .msg_type = V2X_MSG_EMERGENCY,
        .priority = 0,  // Highest priority
        .source_id = get_vehicle_id(),
        .timestamp = k_uptime_get(),
    };
    
    memcpy(msg.data, event, sizeof(struct emergency_event));
    msg.data_len = sizeof(struct emergency_event);
    
    v2x_route_message(&msg);
}
