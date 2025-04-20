#include "road_monitor.h"
#include "v2x_router.h"

#define MAX_ROAD_SEGMENTS 32

static struct road_segment segments[MAX_ROAD_SEGMENTS];
static uint8_t num_segments = 0;

void road_monitor_init(void) {
    memset(segments, 0, sizeof(segments));
}

void update_road_condition(struct road_segment *segment) {
    // Find or add segment
    for (int i = 0; i < num_segments; i++) {
        if (segments[i].segment_id == segment->segment_id) {
            memcpy(&segments[i], segment, sizeof(struct road_segment));
            if (is_hazardous_condition(segment->condition)) {
                broadcast_road_hazard(segment->segment_id, segment->condition);
            }
            return;
        }
    }

    // Add new segment if space available
    if (num_segments < MAX_ROAD_SEGMENTS) {
        memcpy(&segments[num_segments++], segment, sizeof(struct road_segment));
    }
}

void broadcast_road_hazard(uint32_t segment_id, uint8_t hazard_type) {
    struct v2x_message msg = {
        .msg_type = V2X_MSG_ROAD_HAZARD,
        .priority = 1,
        .timestamp = k_uptime_get(),
    };
    
    struct road_hazard_data {
        uint32_t segment_id;
        uint8_t type;
        float location[2];
    } hazard = {
        .segment_id = segment_id,
        .type = hazard_type,
    };
    
    get_segment_location(segment_id, hazard.location);
    memcpy(msg.data, &hazard, sizeof(hazard));
    msg.data_len = sizeof(hazard);
    
    v2x_route_message(&msg);
}
