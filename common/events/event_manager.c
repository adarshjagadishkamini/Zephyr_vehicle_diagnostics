#include "event_manager.h"

#define MAX_SUBSCRIBERS 32
#define MAX_EVENT_QUEUE 64

struct subscriber {
    event_type_t type;
    void (*handler)(struct event *);
};

static struct subscriber subscribers[MAX_SUBSCRIBERS];
static uint8_t num_subscribers = 0;

static struct event event_queue[MAX_EVENT_QUEUE];
static uint8_t queue_head = 0, queue_tail = 0;

K_MUTEX_DEFINE(event_mutex);

void event_manager_init(void) {
    memset(subscribers, 0, sizeof(subscribers));
    memset(event_queue, 0, sizeof(event_queue));
}

int publish_event(struct event *evt) {
    k_mutex_lock(&event_mutex, K_FOREVER);
    
    if (((queue_tail + 1) % MAX_EVENT_QUEUE) == queue_head) {
        k_mutex_unlock(&event_mutex);
        return -ENOMEM;
    }

    memcpy(&event_queue[queue_tail], evt, sizeof(struct event));
    queue_tail = (queue_tail + 1) % MAX_EVENT_QUEUE;
    
    // Notify subscribers
    for (int i = 0; i < num_subscribers; i++) {
        if (subscribers[i].type == evt->type) {
            subscribers[i].handler(evt);
        }
    }
    
    k_mutex_unlock(&event_mutex);
    return 0;
}
