#ifndef CAN_IDS_H
#define CAN_IDS_H

// CAN message IDs
#define CAN_ID_TEMP        0x53
#define CAN_ID_GPS         0x54
#define CAN_ID_COLLISION   0x52
#define CAN_ID_BATTERY     0x51
#define CAN_ID_BRAKE       0x55
#define CAN_ID_TPMS        0x56
#define CAN_ID_SPEED       0x57

// Message lengths
#define TEMP_MSG_LEN      4
#define GPS_MSG_LEN       8
#define COLLISION_MSG_LEN 2
#define BATTERY_MSG_LEN   4
#define BRAKE_MSG_LEN     2
#define TPMS_MSG_LEN      1
#define SPEED_MSG_LEN     2

#endif /* CAN_IDS_H */
