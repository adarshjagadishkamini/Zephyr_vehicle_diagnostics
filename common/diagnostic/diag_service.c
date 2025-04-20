#include "diag_service.h"
#include "secure_storage.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(diag_service, CONFIG_DIAGNOSTIC_LOG_LEVEL);

static struct {
    uint32_t error_count;
    uint32_t last_error_time;
    uint16_t vehicle_status;
    uint8_t security_level;
} diag_ctx;

void diagnostic_service_init(void) {
    memset(&diag_ctx, 0, sizeof(diag_ctx));
    // Initialize diagnostic logging
    init_error_memory();
}

static int pack_vehicle_info(uint8_t *response) {
    struct vehicle_info {
        char vin[17];
        uint16_t model_year;
        uint32_t sw_version;
    } info = {
        .vin = "VIN1234567890ABCD",
        .model_year = 2024,
        .sw_version = VERSION_MAJOR << 16 | VERSION_MINOR << 8 | VERSION_PATCH
    };
    
    memcpy(response, &info, sizeof(info));
    return sizeof(info);
}

static int pack_software_version(uint8_t *response) {
    struct sw_versions {
        uint32_t bootloader;
        uint32_t application;
        uint32_t protocol_stack;
    } versions = {
        .bootloader = BOOTLOADER_VERSION,
        .application = APP_VERSION,
        .protocol_stack = PROTO_VERSION
    };
    
    memcpy(response, &versions, sizeof(versions));
    return sizeof(versions);
}

static int pack_sensor_status(uint8_t *response) {
    uint8_t len = 0;
    for (int i = 0; i < num_sensors; i++) {
        struct sensor_status status;
        get_sensor_status(i, &status);
        memcpy(response + len, &status, sizeof(status));
        len += sizeof(status);
    }
    return len;
}

static int pack_error_memory(uint8_t *response) {
    return get_stored_dtcs(response);
}

static int handle_read_data_by_id(const uint8_t *data, uint16_t len) {
    uint16_t did = (data[0] << 8) | data[1];
    uint8_t response[256];
    uint16_t resp_len = 0;

    switch (did) {
        case DID_VEHICLE_INFO:
            // Pack VIN, model info, etc.
            resp_len = pack_vehicle_info(response);
            break;
            
        case DID_ECU_SW_VERSION:
            // Pack software versions
            resp_len = pack_software_version(response);
            break;
            
        case DID_SENSOR_STATUS:
            // Pack current sensor states
            resp_len = pack_sensor_status(response);
            break;
            
        case DID_ERROR_MEMORY:
            // Pack stored DTCs
            resp_len = pack_error_memory(response);
            break;
    }
    
    // Send diagnostic response
    send_diagnostic_response(UDS_READ_DATA_BY_ID, response, resp_len);
    return 0;
}

static int handle_security_access(const uint8_t *data, uint16_t len) {
    uint8_t level = data[0];
    uint32_t seed, key;
    
    if (len < 5) {
        return -EINVAL;
    }

    // Security level validation
    if (level > MAX_SECURITY_LEVEL) {
        return -EACCES;
    }

    // Calculate response key based on received seed
    seed = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4];
    key = calculate_security_key(seed, level);
    
    // Store security level if key matches
    if (verify_security_key(key, level)) {
        diag_ctx.security_level = level;
        return 0;
    }
    
    return -EACCES;
}

static int execute_self_test(uint8_t control_type) {
    switch (control_type) {
        case ROUTINE_START:
            return start_self_test();
        case ROUTINE_STOP:
            return stop_self_test();
        case ROUTINE_RESULT:
            return get_self_test_result();
    }
    return -EINVAL;
}

static int perform_sensor_calibration(uint8_t control_type) {
    switch (control_type) {
        case ROUTINE_START:
            return start_sensor_calibration();
        case ROUTINE_STOP:
            return stop_sensor_calibration();
        case ROUTINE_RESULT:
            return get_calibration_result();
    }
    return -EINVAL;
}

static int perform_memory_check(uint8_t control_type) {
    switch (control_type) {
        case ROUTINE_START:
            return start_memory_check();
        case ROUTINE_STOP:
            return stop_memory_check();
        case ROUTINE_RESULT:
            return get_memory_check_result();
    }
    return -EINVAL;
}

static int handle_routine_control(const uint8_t *data, uint16_t len) {
    uint16_t routine_id = (data[0] << 8) | data[1];
    uint8_t control_type = data[2];

    switch (routine_id) {
        case ROUTINE_SELF_TEST:
            return execute_self_test(control_type);
            
        case ROUTINE_SENSOR_CALIBRATION:
            return perform_sensor_calibration(control_type);
            
        case ROUTINE_MEMORY_CHECK:
            return perform_memory_check(control_type);
    }
    
    return -EINVAL;
}

int process_diagnostic_request(uint8_t service_id, const uint8_t *data, uint16_t len) {
    switch(service_id) {
        case UDS_READ_DATA_BY_ID:
            return handle_read_data_by_id(data, len);
            
        case UDS_SECURITY_ACCESS:
            return handle_security_access(data, len);
            
        case UDS_ROUTINE_CONTROL:
            return handle_routine_control(data, len);
            
        default:
            return -EINVAL;
    }
}

void update_diagnostic_data(uint16_t did, const void *data, uint16_t len) {
    switch(did) {
        case DID_SENSOR_STATUS:
            update_sensor_status(data, len);
            break;
            
        case DID_ERROR_MEMORY:
            store_error_entry(data, len);
            break;
    }
}
