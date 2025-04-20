#ifndef ROAD_MONITOR_H
#define ROAD_MONITOR_H

#define ROAD_CONDITION_NORMAL  0
#define ROAD_CONDITION_WET     1
#define ROAD_CONDITION_ICY     2
#define ROAD_CONDITION_FOG     3

struct road_segment {
    uint32_t segment_id;
    uint8_t condition;
    float friction;
    float visibility;
    uint32_t last_update;
};

void road_monitor_init(void);
void update_road_condition(struct road_segment *segment);
void broadcast_road_hazard(uint32_t segment_id, uint8_t hazard_type);

#endif /* ROAD_MONITOR_H */
