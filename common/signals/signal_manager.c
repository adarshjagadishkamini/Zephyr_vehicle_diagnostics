#include "signal_manager.h"

#define MAX_SIGNALS 32
#define MAX_CALLBACKS 16

struct signal_callback {
    uint32_t signal_id;
    void (*callback)(signal_t *);
};

static struct signal_callback callbacks[MAX_CALLBACKS];
static uint8_t num_callbacks = 0;

K_MSGQ_DEFINE(signal_queue, sizeof(signal_t), MAX_SIGNALS, 4);

void signal_init(void) {
    memset(callbacks, 0, sizeof(callbacks));
}

int signal_send(signal_t *signal) {
    return k_msgq_put(&signal_queue, signal, K_NO_WAIT);
}

int signal_receive(uint32_t id, signal_t *signal) {
    if (k_msgq_get(&signal_queue, signal, K_FOREVER) == 0) {
        if (signal->id == id) {
            return 0;
        }
    }
    return -EAGAIN;
}

void register_signal_callback(uint32_t id, void (*callback)(signal_t *)) {
    if (num_callbacks < MAX_CALLBACKS) {
        callbacks[num_callbacks].signal_id = id;
        callbacks[num_callbacks].callback = callback;
        num_callbacks++;
    }
}
