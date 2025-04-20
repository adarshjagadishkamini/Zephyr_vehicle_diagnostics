#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

typedef enum {
    EVENT_SENSOR_DATA,
    EVENT_SAFETY_VIOLATION,
    EVENT_COMMUNICATION_ERROR,
    EVENT_SYSTEM_STATE_CHANGE
} event_type_t;

struct event {
    event_type_t type;
    void *data;
    size_t data_len;
};

void event_manager_init(void);
int publish_event(struct event *evt);
int subscribe_to_event(event_type_t type, void (*handler)(struct event *));

#endif /* EVENT_MANAGER_H */
