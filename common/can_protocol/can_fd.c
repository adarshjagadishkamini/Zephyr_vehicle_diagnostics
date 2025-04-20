#include "can_fd.h"

int can_fd_init(const struct device *dev) {
    struct can_timing timing;
    
    // Configure CAN FD timing for data phase
    timing.sjw = 1;
    timing.prop_seg = 0;
    timing.phase_seg1 = 15;
    timing.phase_seg2 = 4;
    timing.prescaler = 1;
    
    can_set_timing(dev, &timing, NULL);
    can_set_mode(dev, CAN_MODE_FD);
    
    return 0;
}

int can_fd_send(const struct device *dev, const struct can_fd_frame *frame) {
    struct can_frame std_frame;
    
    std_frame.id = frame->id;
    std_frame.dlc = frame->len;
    std_frame.flags = CAN_FRAME_FDF | frame->flags;
    memcpy(std_frame.data, frame->data, frame->len);
    
    return can_send(dev, &std_frame, K_MSEC(100), NULL, NULL);
}
