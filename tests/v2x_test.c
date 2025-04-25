#include <zephyr/ztest.h>
#include <zephyr/crypto/crypto.h>
#include "v2x_router.h"
#include "error_handler.h"

static void *test_setup(void) {
    v2x_router_init();
    return NULL;
}

static void test_cleanup(void *data) {
    struct v2x_validation_config default_config = {
        .require_auth = true,
        .require_encryption = false,
        .require_timestamp = true,
        .max_age_ms = V2X_MAX_AGE_MS,
        .min_priority = V2X_MIN_PRIORITY
    };
    v2x_set_validation_config(&default_config);
}

ZTEST_SUITE(v2x_tests, NULL, test_setup, NULL, test_cleanup, NULL);

// Test message validation
ZTEST(v2x_tests, test_message_validation)
{
    struct v2x_message msg = {
        .msg_type = V2X_MSG_EMERGENCY,
        .priority = 0,
        .source_id = 0x12345678,
        .timestamp = k_uptime_get(),
        .flags = V2X_FLAG_TIMESTAMPED | V2X_FLAG_AUTHENTICATED,
        .data_len = 10
    };
    
    // Test valid message
    zassert_equal(v2x_route_message(&msg), 0, "Valid message routing failed");
    
    // Test invalid message type
    msg.msg_type = 0xFF;
    zassert_equal(v2x_route_message(&msg), V2X_ERR_INVALID_TYPE, 
                 "Invalid message type not detected");
    
    // Test invalid size
    msg.msg_type = V2X_MSG_EMERGENCY;
    msg.data_len = V2X_MAX_DATA_SIZE + 1;
    zassert_equal(v2x_route_message(&msg), V2X_ERR_INVALID_SIZE,
                 "Invalid size not detected");
    
    // Test expired message
    msg.data_len = 10;
    msg.timestamp = k_uptime_get() - (V2X_MAX_AGE_MS * 2);
    zassert_equal(v2x_route_message(&msg), V2X_ERR_EXPIRED,
                 "Expired message not detected");
}

// Test message authentication
ZTEST(v2x_tests, test_message_authentication)
{
    struct v2x_message msg = {
        .msg_type = V2X_MSG_EMERGENCY,
        .priority = 0,
        .source_id = 0x12345678,
        .timestamp = k_uptime_get(),
        .flags = V2X_FLAG_TIMESTAMPED,
        .data_len = 10
    };
    
    // Test unauthenticated message with auth required
    struct v2x_validation_config strict_config = {
        .require_auth = true,
        .require_timestamp = true,
        .max_age_ms = V2X_MAX_AGE_MS
    };
    v2x_set_validation_config(&strict_config);
    
    zassert_equal(v2x_route_message(&msg), V2X_ERR_INVALID_AUTH,
                 "Unauthenticated message not rejected");
    
    // Test with authentication
    int ret = v2x_authenticate_message(&msg);
    zassert_equal(ret, 0, "Message authentication failed");
    zassert_true(msg.flags & V2X_FLAG_AUTHENTICATED,
                 "Authentication flag not set");
    
    // Test with invalid HMAC
    msg.hmac[0] ^= 0xFF; // Corrupt HMAC
    ret = v2x_authenticate_message(&msg);
    zassert_equal(ret, V2X_ERR_INVALID_AUTH,
                 "Invalid HMAC not detected");
}

// Test message handlers
ZTEST(v2x_tests, test_message_handlers)
{
    bool handler_called = false;
    void test_handler(struct v2x_message *msg) {
        handler_called = true;
    }
    
    // Register handler
    register_v2x_handler(V2X_MSG_EMERGENCY, test_handler);
    
    // Send test message
    struct v2x_message msg = {
        .msg_type = V2X_MSG_EMERGENCY,
        .priority = 0,
        .source_id = 0x12345678,
        .timestamp = k_uptime_get(),
        .flags = V2X_FLAG_TIMESTAMPED | V2X_FLAG_AUTHENTICATED,
        .data_len = 10
    };
    
    v2x_route_message(&msg);
    zassert_true(handler_called, "Message handler not called");
}

// Test emergency broadcast
ZTEST(v2x_tests, test_emergency_broadcast)
{
    struct emergency_event event = {
        .type = EMERGENCY_COLLISION,
        .severity = SEVERITY_HIGH,
        .latitude = 37.7749,
        .longitude = -122.4194,
        .timestamp = k_uptime_get()
    };
    
    struct v2x_message msg;
    broadcast_emergency_alert(&event);
    
    zassert_true(receive_test_message(&msg), "Message not received");
    zassert_equal(msg.msg_type, V2X_MSG_EMERGENCY, "Wrong message type");
    zassert_equal(msg.priority, 0, "Wrong priority for emergency");
    
    // Verify event data
    struct emergency_event *recv_event = (struct emergency_event *)msg.data;
    zassert_equal(recv_event->type, event.type, "Wrong event type");
    zassert_equal(recv_event->severity, event.severity, "Wrong severity");
}

// Test load handling
ZTEST(v2x_tests, test_load_handling)
{
    struct v2x_message msg = {
        .msg_type = V2X_MSG_TRAFFIC,
        .priority = 1,
        .source_id = 0x12345678,
        .timestamp = k_uptime_get(),
        .flags = V2X_FLAG_TIMESTAMPED | V2X_FLAG_AUTHENTICATED,
        .data_len = 10
    };
    
    // Test rapid message processing
    for (int i = 0; i < 1000; i++) {
        msg.source_id = i;
        msg.timestamp = k_uptime_get();
        zassert_equal(v2x_route_message(&msg), 0,
                     "Message routing failed under load");
        k_sleep(K_MSEC(1));
    }
}

// Test error recovery
ZTEST(v2x_tests, test_error_recovery)
{
    struct v2x_message msg = {
        .msg_type = V2X_MSG_EMERGENCY,
        .priority = 0,
        .source_id = 0x12345678,
        .timestamp = k_uptime_get(),
        .flags = V2X_FLAG_TIMESTAMPED,
        .data_len = 10
    };
    
    // Test authentication retry
    int retries = 0;
    while (retries < MAX_VALIDATION_RETRIES) {
        int ret = v2x_authenticate_message(&msg);
        if (ret == 0) break;
        retries++;
    }
    
    zassert_true(retries < MAX_VALIDATION_RETRIES,
                 "Authentication retry mechanism failed");
}

// Test message priority handling
ZTEST(v2x_tests, test_priority_handling)
{
    struct v2x_validation_config config = {
        .require_auth = true,
        .require_timestamp = true,
        .max_age_ms = V2X_MAX_AGE_MS,
        .min_priority = 2  // Only accept high priority messages
    };
    v2x_set_validation_config(&config);
    
    struct v2x_message msg = {
        .msg_type = V2X_MSG_TRAFFIC,
        .source_id = 0x12345678,
        .timestamp = k_uptime_get(),
        .flags = V2X_FLAG_TIMESTAMPED | V2X_FLAG_AUTHENTICATED,
        .data_len = 10
    };
    
    // Test low priority message
    msg.priority = 3;
    zassert_equal(v2x_route_message(&msg), V2X_ERR_INVALID_PRIORITY,
                 "Low priority message not rejected");
    
    // Test high priority message
    msg.priority = 1;
    zassert_equal(v2x_route_message(&msg), 0,
                 "High priority message rejected");
}
