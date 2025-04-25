#ifndef DIAG_SERVICE_H
#define DIAG_SERVICE_H

#include <zephyr/kernel.h>

// UDS Service IDs
#define UDS_DIAGNOSTIC_SESSION_CONTROL  0x10
#define UDS_ECU_RESET                  0x11
#define UDS_CLEAR_DTC                  0x14
#define UDS_READ_DTC                   0x19
#define UDS_READ_DATA_BY_ID           0x22
#define UDS_READ_MEM_BY_ADDR          0x23
#define UDS_SECURITY_ACCESS           0x27
#define UDS_COMM_CONTROL              0x28
#define UDS_READ_PERIODIC_DATA        0x2A
#define UDS_DYNAMIC_DATA_DEF          0x2C
#define UDS_WRITE_DATA_BY_ID         0x2E
#define UDS_IO_CONTROL               0x2F
#define UDS_ROUTINE_CONTROL          0x31
#define UDS_REQUEST_DOWNLOAD         0x34
#define UDS_REQUEST_UPLOAD           0x35
#define UDS_TRANSFER_DATA            0x36
#define UDS_TRANSFER_EXIT            0x37
#define UDS_WRITE_MEM_BY_ADDR       0x3D
#define UDS_TESTER_PRESENT          0x3E
#define UDS_CONTROL_DTC_SETTING     0x85

// Diagnostic Session Types
#define DIAG_SESSION_DEFAULT         0x01
#define DIAG_SESSION_PROGRAMMING     0x02
#define DIAG_SESSION_EXTENDED        0x03
#define DIAG_SESSION_SAFETY          0x04

// Security Access Levels
#define SEC_LEVEL_UNLOCK_DIAG        0x01
#define SEC_LEVEL_UNLOCK_PROG        0x03
#define SEC_LEVEL_UNLOCK_EXTENDED    0x05
#define SEC_LEVEL_UNLOCK_SAFETY      0x07

// Routine IDs
#define ROUTINE_SELF_TEST            0x0100
#define ROUTINE_SENSOR_CALIBRATION   0x0200
#define ROUTINE_MEMORY_CHECK         0x0300
#define ROUTINE_SECURITY_CHECK       0x0400
#define ROUTINE_ERASE_MEMORY         0x0500
#define ROUTINE_CHECK_PROG_DEP       0x0600
#define ROUTINE_SYSTEM_MONITOR       0x0700

// Routine Control Types
#define ROUTINE_START                0x01
#define ROUTINE_STOP                 0x02
#define ROUTINE_RESULT               0x03

// Response Codes
#define DIAG_RESP_OK                 0x00
#define DIAG_RESP_GEN_REJECT        0x10
#define DIAG_RESP_SERVICE_NA        0x11
#define DIAG_RESP_SUBFUNC_NA        0x12
#define DIAG_RESP_INCORRECT_LENGTH  0x13
#define DIAG_RESP_BUSY              0x21
#define DIAG_RESP_CONDITIONS_NA     0x22
#define DIAG_RESP_REQUEST_SEQ_ERR   0x24
#define DIAG_RESP_SECURITY_DENIED   0x33
#define DIAG_RESP_INVALID_KEY       0x35
#define DIAG_RESP_TOO_MANY_ATT      0x36
#define DIAG_RESP_REQUIRED_TIME_NA  0x37

// Communication Control Types
#define COMM_ENABLE_RX_TX           0x00
#define COMM_ENABLE_RX_DISABLE_TX   0x01
#define COMM_DISABLE_RX_ENABLE_TX   0x02
#define COMM_DISABLE_RX_TX          0x03

// Communication Types
#define COMM_NORMAL                 0x01
#define COMM_NETWORK_MANAGEMENT     0x02
#define COMM_NORMAL_AND_NM          0x03

// Diagnostic Data Structures
struct diag_session_info {
    uint8_t session_type;
    uint16_t p2_server_max;
    uint16_t p2_star_server_max;
};

struct security_seed_params {
    uint8_t level;
    uint32_t seed;
    uint32_t timestamp;
};

struct routine_status {
    uint16_t routine_id;
    uint8_t status;
    uint16_t result_code;
    uint8_t data[256];
    uint16_t data_len;
};

// Function Prototypes
void diagnostic_service_init(void);
int process_diagnostic_request(uint8_t service_id, const uint8_t *data, uint16_t len);
void update_diagnostic_data(uint16_t did, const void *data, uint16_t len);
int start_diagnostic_session(uint8_t session_type);
int verify_security_access(uint8_t level, uint32_t key);
int execute_routine(uint16_t routine_id, uint8_t control_type, const uint8_t *data, uint16_t len);
int control_dtc_settings(uint8_t dtc_setting);
int control_communication(uint8_t control_type, uint8_t comm_type);
const char *get_diag_error_string(uint8_t response_code);

#endif /* DIAG_SERVICE_H */
