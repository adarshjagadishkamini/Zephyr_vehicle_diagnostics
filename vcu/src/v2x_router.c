#include "v2x_router.h"

#define MAX_HANDLERS 16

struct v2x_handler {
    uint8_t msg_type;
    void (*handler)(struct v2x_message *);
};

static struct v2x_handler handlers[MAX_HANDLERS];
static uint8_t num_handlers = 0;

void v2x_router_init(void) {
    memset(handlers, 0, sizeof(handlers));
}

int v2x_route_message(struct v2x_message *msg) {
    for (int i = 0; i < num_handlers; i++) {
        if (handlers[i].msg_type == msg->msg_type) {
            handlers[i].handler(msg);
            return 0;
        }
    }
    return -ENOENT;
}

void register_v2x_handler(uint8_t msg_type, void (*handler)(struct v2x_message *)) {
    if (num_handlers < MAX_HANDLERS) {
        handlers[num_handlers].msg_type = msg_type;
        handlers[num_handlers].handler = handler;
        num_handlers++;
    }
}
