#include "signal_manager.h"

#define MAX_SIGNALS 32
#define MAX_CALLBACKS 16
#define SIGNAL_PRIORITY 5
#define SIGNAL_STACK_SIZE 1024

struct signal_callback {
    uint32_t signal_id;
    void (*callback)(signal_t *);
};

static struct signal_callback callbacks[MAX_CALLBACKS];
static uint8_t num_callbacks = 0;

K_MSGQ_DEFINE(signal_queue, sizeof(signal_t), MAX_SIGNALS, 4);

K_THREAD_STACK_DEFINE(signal_stack, SIGNAL_STACK_SIZE);
static struct k_thread signal_thread;

static void signal_processing_thread(void *p1, void *p2, void *p3) {
    signal_t signal;
    
    while (1) {
        if (k_msgq_get(&signal_queue, &signal, K_FOREVER) == 0) {
            // Process callbacks
            for (int i = 0; i < num_callbacks; i++) {
                if (callbacks[i].signal_id == signal.id) {
                    callbacks[i].callback(&signal);
                }
            }
        }
    }
}

void signal_init(void) {
    memset(callbacks, 0, sizeof(callbacks));
    
    k_thread_create(&signal_thread, signal_stack,
                    K_THREAD_STACK_SIZEOF(signal_stack),
                    signal_processing_thread,
                    NULL, NULL, NULL,
                    SIGNAL_PRIORITY, 0, K_NO_WAIT);
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
