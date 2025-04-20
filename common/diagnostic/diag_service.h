#ifndef DIAG_SERVICE_H
#define DIAG_SERVICE_H

#include <zephyr/kernel.h>

// UDS Service IDs
#define UDS_READ_DATA_BY_ID           0x22
#define UDS_SECURITY_ACCESS           0x27
#define UDS_ROUTINE_CONTROL          0x31
#define UDS_REQUEST_DOWNLOAD         0x34
#define UDS_TRANSFER_DATA            0x36

// Diagnostic Data IDs
#define DID_VEHICLE_INFO             0xF190
#define DID_ECU_SW_VERSION          0xF183
#define DID_SENSOR_STATUS           0xF120
#define DID_ERROR_MEMORY            0xF150

void diagnostic_service_init(void);
int process_diagnostic_request(uint8_t service_id, const uint8_t *data, uint16_t len);
void update_diagnostic_data(uint16_t did, const void *data, uint16_t len);

#endif /* DIAG_SERVICE_H */
