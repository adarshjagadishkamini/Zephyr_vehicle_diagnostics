#ifndef V2X_ROUTER_H
#define V2X_ROUTER_H

// Message types
#define V2X_MSG_EMERGENCY    0x01
#define V2X_MSG_TRAFFIC      0x02
#define V2X_MSG_WEATHER      0x03
#define V2X_MSG_ROAD_HAZARD  0x04

struct v2x_message {
    uint8_t msg_type;
    uint8_t priority;
    uint32_t source_id;
    uint32_t timestamp;
    uint8_t data[256];
    uint16_t data_len;
};

void v2x_router_init(void);
int v2x_route_message(struct v2x_message *msg);
void register_v2x_handler(uint8_t msg_type, void (*handler)(struct v2x_message *));

#endif /* V2X_ROUTER_H */
