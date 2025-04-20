#ifndef SIGNAL_MANAGER_H
#define SIGNAL_MANAGER_H

typedef struct {
    uint32_t id;
    uint8_t *data;
    size_t len;
    uint32_t sender;
    uint32_t receiver;
} signal_t;

void signal_init(void);
int signal_send(signal_t *signal);
int signal_receive(uint32_t id, signal_t *signal);
void register_signal_callback(uint32_t id, void (*callback)(signal_t *));

#endif /* SIGNAL_MANAGER_H */
