#include <zephyr/ztest.h>
#include "v2x_router.h"

ZTEST_SUITE(v2x_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(v2x_tests, test_message_routing)
{
    struct v2x_message test_msg = {
        .msg_type = V2X_MSG_EMERGENCY,
        .priority = 0,
        .data_len = 10
    };
    
    zassert_equal(v2x_route_message(&test_msg), 0, "Message routing failed");
}
