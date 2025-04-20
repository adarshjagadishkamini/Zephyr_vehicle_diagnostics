#include <zephyr/ztest.h>
#include "v2x_router.h"

static void *test_setup(void) {
    v2x_router_init();
    return NULL;
}

ZTEST_SUITE(v2x_tests, NULL, test_setup, NULL, NULL, NULL);

ZTEST(v2x_tests, test_message_routing)
{
    struct v2x_message test_msg = {
        .msg_type = V2X_MSG_EMERGENCY,
        .priority = 0,
        .data_len = 10
    };
    
    zassert_equal(v2x_route_message(&test_msg), 0, "Message routing failed");
}

ZTEST(v2x_tests, test_emergency_broadcast) {
    struct emergency_event event = {
        .type = EMERGENCY_COLLISION,
        .severity = SEVERITY_HIGH
    };
    
    struct v2x_message msg;
    broadcast_emergency_alert(&event);
    
    zassert_true(receive_test_message(&msg), "Message not received");
    zassert_equal(msg.msg_type, V2X_MSG_EMERGENCY, "Wrong message type");
}
