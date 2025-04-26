#include "v2x_router.h"
#include <zephyr/kernel.h>

#define CONFIG_V2X_EXPERIMENTAL 0  // Disable experimental features by default

void v2x_route_message(struct v2x_message *msg) {
#if CONFIG_V2X_EXPERIMENTAL
    switch(msg->msg_type) {
        case V2X_MSG_SAFETY:
            handle_safety_message(msg);
            break;
        case V2X_MSG_TRAFFIC:
            handle_traffic_message(msg);
            break;
        case V2X_MSG_INFRASTRUCTURE:
            handle_infrastructure_message(msg);
            break;
    }
#else
    // In non-experimental mode, only handle essential safety messages
    if (msg->msg_type == V2X_MSG_SAFETY && msg->priority > V2X_PRIORITY_HIGH) {
        handle_safety_message(msg);
    }
#endif
}
