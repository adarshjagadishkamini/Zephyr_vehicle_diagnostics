#include "infrastructure_manager.h"
#include "mqtt_handler.h"

#define MAX_INFRASTRUCTURE_NODES 16

struct infrastructure_node {
    uint8_t id;
    uint8_t type;
    uint32_t last_update;
    union {
        struct {
            uint8_t state;
            uint32_t timing;
        } traffic_light;
        struct {
            uint8_t condition;
            float friction;
        } road;
    } data;
};

static struct infrastructure_node nodes[MAX_INFRASTRUCTURE_NODES];

void infrastructure_init(void) {
    memset(nodes, 0, sizeof(nodes));
    subscribe_to_infrastructure_topics();
}

void process_traffic_signal(uint8_t signal_id, uint8_t state) {
    // Process and store traffic signal state
    // Update V2X system with traffic information
    char json[128];
    snprintf(json, sizeof(json), 
             "{\"type\":\"traffic\",\"id\":%d,\"state\":%d}", 
             signal_id, state);
    publish_to_topic(TOPIC_V2I, json);
}
