#ifndef V2X_HANDLER_H
#define V2X_HANDLER_H

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>

// V2V message types
#define V2V_GPS_DATA      0x01
#define V2V_HAZARD_DATA   0x02
#define V2V_BRAKE_DATA    0x03
#define V2V_SPEED_DATA    0x04

void v2x_init(void);
int broadcast_v2v_data(uint8_t type, const uint8_t *data, uint16_t len);
void process_v2v_message(const uint8_t *data, uint16_t len);

#endif /* V2X_HANDLER_H */
