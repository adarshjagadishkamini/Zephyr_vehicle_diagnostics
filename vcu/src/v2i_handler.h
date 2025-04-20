#ifndef V2I_HANDLER_H
#define V2I_HANDLER_H

#include <zephyr/net/mqtt.h>

// V2I message types
#define V2I_TRAFFIC_LIGHT    0x01
#define V2I_ROAD_CONDITION   0x02
#define V2I_TRAFFIC_FLOW     0x03

void v2i_init(void);
int send_v2i_data(uint8_t type, const uint8_t *data, uint16_t len);
void process_v2i_message(const uint8_t *data, uint16_t len);

#endif /* V2I_HANDLER_H */
