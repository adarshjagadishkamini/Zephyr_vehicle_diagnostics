#ifndef CAN_AUTH_H
#define CAN_AUTH_H

#include <zephyr/drivers/can.h>

// CMAC based authentication
int authenticate_can_message(struct can_frame *frame);
int verify_can_message(struct can_frame *frame);

#endif /* CAN_AUTH_H */
