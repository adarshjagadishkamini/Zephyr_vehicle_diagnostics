#ifndef J1939_H
#define J1939_H

#include <zephyr/drivers/can.h>

// Standard J1939 PGNs
#define J1939_PGN_ENGINE_TEMP        0xFEE6  // Engine Temperature
#define J1939_PGN_VEHICLE_SPEED      0xFEF1  // Vehicle Speed
#define J1939_PGN_BRAKE_INFO         0xFE4E  // Brake Information
#define J1939_PGN_TPMS               0xFE4F  // Tire Pressure Monitoring
#define J1939_PGN_VEHICLE_POSITION   0xFEF3  // GPS Position Data
#define J1939_PGN_VIN                0xFEEC  // Vehicle Identification
#define J1939_PGN_DIAGNOSTIC         0xFECA  // Diagnostic Message
#define J1939_PGN_BATTERY_STATUS     0xFEF4  // Battery Status
#define J1939_PGN_COLLISION_WARN     0xFEC5  // Collision Warning
#define J1939_PGN_FAULT_INFO         0xFECE  // Fault Information

// Transport Protocol PGNs
#define J1939_PGN_TP_CM             0xEC00  // Transport Protocol - Connection Management
#define J1939_PGN_TP_DT             0xEB00  // Transport Protocol - Data Transfer

// Transport Protocol commands
#define TP_CM_RTS                   0x10    // Request to Send
#define TP_CM_CTS                   0x11    // Clear to Send
#define TP_CM_EndOfMsgAck          0x13    // End of Message Acknowledgement
#define TP_CM_BAM                   0x20    // Broadcast Announce Message
#define TP_CM_Abort                 0xFF    // Connection Abort

// J1939 priorities
#define J1939_PRIORITY_HIGH         0x00
#define J1939_PRIORITY_MEDIUM       0x03
#define J1939_PRIORITY_LOW          0x07

struct j1939_ctx {
    const struct device *can_dev;
    uint8_t source_address;
    uint8_t dest_address;
    struct {
        uint32_t pgn;
        uint16_t total_size;
        uint8_t num_packets;
        uint8_t next_packet;
        uint8_t *data;
    } tp_session;
    bool tp_in_progress;
};

// Function prototypes
int j1939_init(struct j1939_ctx *ctx);
int j1939_send_pgn(struct j1939_ctx *ctx, uint32_t pgn, const uint8_t *data, uint8_t len);
int j1939_send_tp_data(struct j1939_ctx *ctx, uint32_t pgn, const uint8_t *data, uint16_t len);
void j1939_process_message(struct j1939_ctx *ctx, struct can_frame *frame);
int j1939_register_pgn_handler(uint32_t pgn, void (*handler)(struct j1939_ctx *, const uint8_t *, uint16_t));

// Error codes
#define J1939_ERR_TP_TIMEOUT       -1
#define J1939_ERR_TP_ABORT         -2
#define J1939_ERR_INVALID_PGN      -3
#define J1939_ERR_BUFFER_FULL      -4

#endif /* J1939_H */
