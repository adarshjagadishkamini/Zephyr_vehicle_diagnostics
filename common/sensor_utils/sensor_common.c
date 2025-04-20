#include "sensor_common.h"

node_error_t sensor_init(const struct device *dev) {
    if (!device_is_ready(dev)) {
        return NODE_ERROR_SENSOR_INIT;
    }
    return NODE_SUCCESS;
}

int send_sensor_data(const struct device *can_dev, uint32_t id, 
                    const uint8_t *data, uint8_t len) {
    struct can_frame frame = {
        .id = id,
        .dlc = len,
    };
    memcpy(frame.data, data, len);
    
    return can_send(can_dev, &frame, K_MSEC(100), NULL, NULL);
}
