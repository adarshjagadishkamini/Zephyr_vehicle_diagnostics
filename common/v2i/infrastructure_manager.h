#ifndef INFRASTRUCTURE_MANAGER_H
#define INFRASTRUCTURE_MANAGER_H

// Traffic light states
#define TRAFFIC_LIGHT_RED     0
#define TRAFFIC_LIGHT_YELLOW  1
#define TRAFFIC_LIGHT_GREEN   2

// Road conditions
#define ROAD_CONDITION_NORMAL 0
#define ROAD_CONDITION_WET    1
#define ROAD_CONDITION_ICY    2

void infrastructure_init(void);
void process_traffic_signal(uint8_t signal_id, uint8_t state);
void process_road_condition(uint8_t segment_id, uint8_t condition);
void notify_infrastructure_event(uint8_t event_type, const void *data);

#endif /* INFRASTRUCTURE_MANAGER_H */
