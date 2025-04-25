#include <zephyr/ztest.h>
#include "diag_service.h"
#include "error_handler.h"

static void *test_setup(void) {
    diagnostic_service_init();
    return NULL;
}

ZTEST_SUITE(diagnostic_tests, NULL, test_setup, NULL, NULL, NULL);

// Test ReadDataByIdentifier service
ZTEST(diagnostic_tests, test_read_data_by_id)
{
    uint8_t request[3] = {UDS_READ_DATA_BY_ID, 0xF1, 0x90}; // Vehicle Info DID
    uint8_t response[256];
    uint16_t resp_len;
    
    int ret = process_diagnostic_request(UDS_READ_DATA_BY_ID, request, sizeof(request));
    zassert_equal(ret, 0, "ReadDataById request failed");
    
    // Test invalid DID
    request[1] = 0xFF;
    request[2] = 0xFF;
    ret = process_diagnostic_request(UDS_READ_DATA_BY_ID, request, sizeof(request));
    zassert_not_equal(ret, 0, "Invalid DID not detected");
}

// Test SecurityAccess service
ZTEST(diagnostic_tests, test_security_access)
{
    uint8_t request[6] = {UDS_SECURITY_ACCESS, 0x01}; // Request seed level 1
    uint32_t seed;
    uint32_t key;
    
    // Test seed request
    int ret = process_diagnostic_request(UDS_SECURITY_ACCESS, request, 2);
    zassert_equal(ret, 0, "Security seed request failed");
    
    // Get seed from response and calculate key
    memcpy(&seed, &request[2], 4);
    key = calculate_security_key(seed, 0x01);
    
    // Send key
    request[0] = UDS_SECURITY_ACCESS;
    request[1] = 0x02; // Send key
    memcpy(&request[2], &key, 4);
    ret = process_diagnostic_request(UDS_SECURITY_ACCESS, request, 6);
    zassert_equal(ret, 0, "Security key validation failed");
    
    // Test invalid key
    key ^= 0xFFFFFFFF; // Corrupt key
    memcpy(&request[2], &key, 4);
    ret = process_diagnostic_request(UDS_SECURITY_ACCESS, request, 6);
    zassert_not_equal(ret, 0, "Invalid key not detected");
}

// Test RoutineControl service
ZTEST(diagnostic_tests, test_routine_control)
{
    // Test self-test routine
    uint8_t request[4] = {
        UDS_ROUTINE_CONTROL,
        ROUTINE_START,
        ROUTINE_SELF_TEST >> 8,
        ROUTINE_SELF_TEST & 0xFF
    };
    
    int ret = process_diagnostic_request(UDS_ROUTINE_CONTROL, request, sizeof(request));
    zassert_equal(ret, 0, "Routine start failed");
    
    // Wait for routine completion
    k_sleep(K_MSEC(100));
    
    // Get results
    request[1] = ROUTINE_RESULT;
    ret = process_diagnostic_request(UDS_ROUTINE_CONTROL, request, sizeof(request));
    zassert_equal(ret, 0, "Failed to get routine results");
    
    // Test invalid routine
    request[2] = 0xFF;
    request[3] = 0xFF;
    ret = process_diagnostic_request(UDS_ROUTINE_CONTROL, request, sizeof(request));
    zassert_not_equal(ret, 0, "Invalid routine not detected");
}

// Test CommunicationControl service
ZTEST(diagnostic_tests, test_communication_control)
{
    uint8_t request[3] = {
        UDS_COMM_CONTROL,
        COMM_DISABLE_RX_TX,
        COMM_NORMAL
    };
    
    int ret = process_diagnostic_request(UDS_COMM_CONTROL, request, sizeof(request));
    zassert_equal(ret, 0, "Communication control failed");
    
    // Verify communication is disabled
    zassert_false(is_communication_enabled(), "Communication not disabled");
    
    // Re-enable communication
    request[1] = COMM_ENABLE_RX_TX;
    ret = process_diagnostic_request(UDS_COMM_CONTROL, request, sizeof(request));
    zassert_equal(ret, 0, "Failed to re-enable communication");
    
    zassert_true(is_communication_enabled(), "Communication not re-enabled");
}

// Test ReadDTC service
ZTEST(diagnostic_tests, test_read_dtc)
{
    uint8_t request[2] = {UDS_READ_DTC, 0x01}; // Report DTCs
    uint8_t response[256];
    uint16_t resp_len;
    
    // Store a test DTC
    store_dtc(0x123456, DTC_ACTIVE);
    
    int ret = process_diagnostic_request(UDS_READ_DTC, request, sizeof(request));
    zassert_equal(ret, 0, "ReadDTC request failed");
    
    // Verify DTC is reported
    bool dtc_found = false;
    for (int i = 0; i < resp_len; i += 4) {
        if (*(uint32_t*)&response[i] == 0x123456) {
            dtc_found = true;
            break;
        }
    }
    zassert_true(dtc_found, "Stored DTC not reported");
}

// Test ClearDTC service
ZTEST(diagnostic_tests, test_clear_dtc)
{
    uint8_t request[4] = {UDS_CLEAR_DTC, 0xFF, 0xFF, 0xFF}; // Clear all DTCs
    
    // Store some test DTCs
    store_dtc(0x123456, DTC_ACTIVE);
    store_dtc(0x789ABC, DTC_ACTIVE);
    
    int ret = process_diagnostic_request(UDS_CLEAR_DTC, request, sizeof(request));
    zassert_equal(ret, 0, "ClearDTC request failed");
    
    // Verify DTCs are cleared
    uint8_t read_request[2] = {UDS_READ_DTC, 0x01};
    uint8_t response[256];
    uint16_t resp_len;
    
    ret = process_diagnostic_request(UDS_READ_DTC, read_request, sizeof(read_request));
    zassert_equal(ret, 0, "ReadDTC request failed");
    zassert_equal(resp_len, 0, "DTCs not cleared");
}

// Test error recovery and session handling
ZTEST(diagnostic_tests, test_error_recovery)
{
    // Test session timeout recovery
    uint8_t request[2] = {UDS_DIAGNOSTIC_SESSION_CONTROL, DIAG_SESSION_PROGRAMMING};
    
    int ret = process_diagnostic_request(UDS_DIAGNOSTIC_SESSION_CONTROL, request, sizeof(request));
    zassert_equal(ret, 0, "Session change failed");
    
    // Wait for session timeout
    k_sleep(K_SECONDS(5));
    
    // Verify fallback to default session
    zassert_equal(get_current_session(), DIAG_SESSION_DEFAULT, 
                 "Session timeout recovery failed");
    
    // Test security access lockout
    uint8_t sec_request[6] = {UDS_SECURITY_ACCESS, 0x01};
    for (int i = 0; i < 5; i++) {
        ret = process_diagnostic_request(UDS_SECURITY_ACCESS, sec_request, 2);
        zassert_equal(ret, 0, "Security seed request failed");
        
        // Send invalid key
        sec_request[1] = 0x02;
        ret = process_diagnostic_request(UDS_SECURITY_ACCESS, sec_request, 6);
    }
    
    // Verify lockout
    ret = process_diagnostic_request(UDS_SECURITY_ACCESS, sec_request, 2);
    zassert_not_equal(ret, 0, "Security lockout not enforced");
    
    // Wait for lockout timer
    k_sleep(K_SECONDS(10));
    
    // Verify lockout cleared
    ret = process_diagnostic_request(UDS_SECURITY_ACCESS, sec_request, 2);
    zassert_equal(ret, 0, "Security lockout not cleared after timeout");
}

// Test load handling
ZTEST(diagnostic_tests, test_load_handling)
{
    uint8_t request[3] = {UDS_READ_DATA_BY_ID, 0xF1, 0x90};
    
    // Test rapid requests
    for (int i = 0; i < 1000; i++) {
        int ret = process_diagnostic_request(UDS_READ_DATA_BY_ID, request, sizeof(request));
        zassert_equal(ret, 0, "Request failed under load");
        k_sleep(K_MSEC(1));
    }
}