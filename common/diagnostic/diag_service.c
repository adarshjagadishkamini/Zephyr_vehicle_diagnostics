#include <zephyr/kernel.h>
#include <zephyr/crypto/crypto.h>
#include <string.h>
#include "diag_service.h"
#include "secure_storage.h"
#include "error_handler.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(diag_service, CONFIG_DIAGNOSTIC_LOG_LEVEL);

#define MAX_SECURITY_ATTEMPTS 3
#define SECURITY_LOCKOUT_TIME_MS 10000
#define MAX_TRANSFER_SIZE 4096
#define MAX_PERIODIC_DIDS 16

struct diag_context {
    uint8_t current_session;
    uint8_t security_level;
    uint8_t security_attempts;
    uint32_t last_security_attempt;
    bool dtc_settings_enabled;
    struct routine_status active_routine;
    struct {
        uint16_t did;
        uint16_t rate_ms;
    } periodic_dids[MAX_PERIODIC_DIDS];
    uint8_t num_periodic_dids;
    K_MUTEX_DEFINE(context_lock);
};

static struct diag_context diag_ctx;
static uint8_t transfer_buffer[MAX_TRANSFER_SIZE];
static uint32_t transfer_size;
static uint32_t transfer_offset;

void diagnostic_service_init(void) {
    memset(&diag_ctx, 0, sizeof(diag_ctx));
    diag_ctx.current_session = DIAG_SESSION_DEFAULT;
    diag_ctx.dtc_settings_enabled = true;
    memset(transfer_buffer, 0, sizeof(transfer_buffer));
}

static int validate_session_transition(uint8_t new_session) {
    // Check if transition is allowed from current session
    switch (diag_ctx.current_session) {
        case DIAG_SESSION_DEFAULT:
            return true; // Can transition to any session
            
        case DIAG_SESSION_PROGRAMMING:
            if (new_session == DIAG_SESSION_DEFAULT)
                return true;
            return false;
            
        case DIAG_SESSION_EXTENDED:
        case DIAG_SESSION_SAFETY:
            if (new_session == DIAG_SESSION_DEFAULT || 
                new_session == DIAG_SESSION_PROGRAMMING)
                return true;
            return false;
    }
    return false;
}

int start_diagnostic_session(uint8_t session_type) {
    if (session_type < DIAG_SESSION_DEFAULT || session_type > DIAG_SESSION_SAFETY) {
        return DIAG_RESP_SUBFUNC_NA;
    }
    
    if (!validate_session_transition(session_type)) {
        return DIAG_RESP_CONDITIONS_NA;
    }
    
    k_mutex_lock(&diag_ctx.context_lock, K_FOREVER);
    diag_ctx.current_session = session_type;
    diag_ctx.security_level = 0; // Reset security on session change
    k_mutex_unlock(&diag_ctx.context_lock);
    
    return DIAG_RESP_OK;
}

static uint32_t generate_security_seed(uint8_t level) {
    uint32_t timestamp = k_uptime_get_32();
    uint32_t seed;
    
    // Generate seed using timestamp and random number
    sys_rand_get((uint8_t *)&seed, sizeof(seed));
    seed ^= timestamp;
    seed ^= (uint32_t)level << 24;
    
    return seed;
}

static uint32_t calculate_security_key(uint32_t seed, uint8_t level) {
    // Use a more secure key derivation approach
    uint32_t key = seed;
    for (int i = 0; i < 8; i++) {
        key ^= (key << 13) | (key >> 19);  // Left rotation
        key *= 0x85ebca6b;  // Multiplication with prime number
        key ^= key >> 16;
        key *= 0xc2b2ae35;  // Another prime multiplication
        key ^= (uint32_t)level << (8 * (i % 4));  // Mix in security level
    }
    return key;
}

int verify_security_access(uint8_t level, uint32_t key) {
    uint32_t current_time = k_uptime_get_32();
    
    k_mutex_lock(&diag_ctx.context_lock, K_FOREVER);
    
    // Check lockout
    if (diag_ctx.security_attempts >= MAX_SECURITY_ATTEMPTS) {
        if (current_time - diag_ctx.last_security_attempt < SECURITY_LOCKOUT_TIME_MS) {
            k_mutex_unlock(&diag_ctx.context_lock);
            return DIAG_RESP_REQUIRED_TIME_NA;
        }
        diag_ctx.security_attempts = 0;
    }
    
    // Verify security access
    uint32_t expected_key = calculate_security_key(diag_ctx.last_security_attempt, level);
    if (key != expected_key) {
        diag_ctx.security_attempts++;
        diag_ctx.last_security_attempt = current_time;
        k_mutex_unlock(&diag_ctx.context_lock);
        return DIAG_RESP_INVALID_KEY;
    }
    
    diag_ctx.security_level = level;
    diag_ctx.security_attempts = 0;
    k_mutex_unlock(&diag_ctx.context_lock);
    
    return DIAG_RESP_OK;
}

static int handle_routine_start(uint16_t routine_id, const uint8_t *data, uint16_t len) {
    k_mutex_lock(&diag_ctx.context_lock, K_FOREVER);
    
    if (diag_ctx.active_routine.status != 0) {
        k_mutex_unlock(&diag_ctx.context_lock);
        return DIAG_RESP_CONDITIONS_NA;
    }
    
    diag_ctx.active_routine.routine_id = routine_id;
    diag_ctx.active_routine.status = 1;
    memcpy(diag_ctx.active_routine.data, data, len);
    diag_ctx.active_routine.data_len = len;
    
    k_mutex_unlock(&diag_ctx.context_lock);
    
    // Start specific routine
    switch (routine_id) {
        case ROUTINE_SELF_TEST:
            return execute_self_test();
        case ROUTINE_SENSOR_CALIBRATION:
            return execute_sensor_calibration();
        case ROUTINE_MEMORY_CHECK:
            return execute_memory_check();
        case ROUTINE_SECURITY_CHECK:
            return execute_security_check();
        default:
            return DIAG_RESP_SUBFUNC_NA;
    }
}

int execute_routine(uint16_t routine_id, uint8_t control_type, const uint8_t *data, uint16_t len) {
    switch (control_type) {
        case ROUTINE_START:
            return handle_routine_start(routine_id, data, len);
            
        case ROUTINE_STOP:
            k_mutex_lock(&diag_ctx.context_lock, K_FOREVER);
            if (diag_ctx.active_routine.routine_id != routine_id) {
                k_mutex_unlock(&diag_ctx.context_lock);
                return DIAG_RESP_CONDITIONS_NA;
            }
            diag_ctx.active_routine.status = 0;
            k_mutex_unlock(&diag_ctx.context_lock);
            return DIAG_RESP_OK;
            
        case ROUTINE_RESULT:
            k_mutex_lock(&diag_ctx.context_lock, K_FOREVER);
            if (diag_ctx.active_routine.routine_id != routine_id) {
                k_mutex_unlock(&diag_ctx.context_lock);
                return DIAG_RESP_CONDITIONS_NA;
            }
            // Return routine results
            memcpy((void *)data, diag_ctx.active_routine.data, 
                   diag_ctx.active_routine.data_len);
            k_mutex_unlock(&diag_ctx.context_lock);
            return DIAG_RESP_OK;
            
        default:
            return DIAG_RESP_SUBFUNC_NA;
    }
}

int control_dtc_settings(uint8_t dtc_setting) {
    k_mutex_lock(&diag_ctx.context_lock, K_FOREVER);
    diag_ctx.dtc_settings_enabled = dtc_setting != 0;
    k_mutex_unlock(&diag_ctx.context_lock);
    return DIAG_RESP_OK;
}

int control_communication(uint8_t control_type, uint8_t comm_type) {
    if (control_type > COMM_DISABLE_RX_TX || comm_type > COMM_NORMAL_AND_NM) {
        return DIAG_RESP_SUBFUNC_NA;
    }
    
    // Implement communication control based on type
    switch (control_type) {
        case COMM_ENABLE_RX_TX:
            enable_network_communication(comm_type);
            break;
        case COMM_ENABLE_RX_DISABLE_TX:
            disable_transmission(comm_type);
            break;
        case COMM_DISABLE_RX_ENABLE_TX:
            disable_reception(comm_type);
            break;
        case COMM_DISABLE_RX_TX:
            disable_network_communication(comm_type);
            break;
    }
    
    return DIAG_RESP_OK;
}

int process_diagnostic_request(uint8_t service_id, const uint8_t *data, uint16_t len) {
    int ret;
    
    // Validate session requirements
    if (!validate_service_in_session(service_id, diag_ctx.current_session)) {
        return DIAG_RESP_SERVICE_NA;
    }
    
    // Process service request
    switch (service_id) {
        case UDS_DIAGNOSTIC_SESSION_CONTROL:
            if (len < 1) return DIAG_RESP_INCORRECT_LENGTH;
            return start_diagnostic_session(data[0]);
            
        case UDS_SECURITY_ACCESS:
            if (len < 5) return DIAG_RESP_INCORRECT_LENGTH;
            return verify_security_access(data[0], *(uint32_t *)&data[1]);
            
        case UDS_READ_DATA_BY_ID:
            if (len < 2) return DIAG_RESP_INCORRECT_LENGTH;
            return handle_read_data_by_id(data, len);
            
        case UDS_WRITE_DATA_BY_ID:
            if (len < 3) return DIAG_RESP_INCORRECT_LENGTH;
            return handle_write_data_by_id(data, len);
            
        case UDS_ROUTINE_CONTROL:
            if (len < 3) return DIAG_RESP_INCORRECT_LENGTH;
            return execute_routine(*(uint16_t *)data, data[2], &data[3], len - 3);
            
        case UDS_REQUEST_DOWNLOAD:
            return handle_request_download(data, len);
            
        case UDS_TRANSFER_DATA:
            return handle_transfer_data(data, len);
            
        case UDS_TRANSFER_EXIT:
            return handle_transfer_exit();
            
        case UDS_TESTER_PRESENT:
            return DIAG_RESP_OK;
            
        case UDS_CONTROL_DTC_SETTING:
            if (len < 1) return DIAG_RESP_INCORRECT_LENGTH;
            return control_dtc_settings(data[0]);
            
        case UDS_COMM_CONTROL:
            if (len < 2) return DIAG_RESP_INCORRECT_LENGTH;
            return control_communication(data[0], data[1]);
            
        default:
            return DIAG_RESP_SERVICE_NA;
    }
}

const char *get_diag_error_string(uint8_t response_code) {
    switch (response_code) {
        case DIAG_RESP_OK:
            return "Success";
        case DIAG_RESP_GEN_REJECT:
            return "General reject";
        case DIAG_RESP_SERVICE_NA:
            return "Service not available";
        case DIAG_RESP_SUBFUNC_NA:
            return "Sub-function not available";
        case DIAG_RESP_BUSY:
            return "Busy, repeat request";
        case DIAG_RESP_CONDITIONS_NA:
            return "Conditions not correct";
        case DIAG_RESP_SECURITY_DENIED:
            return "Security access denied";
        case DIAG_RESP_INVALID_KEY:
            return "Invalid security key";
        case DIAG_RESP_TOO_MANY_ATT:
            return "Too many attempts";
        default:
            return "Unknown error";
    }
}
