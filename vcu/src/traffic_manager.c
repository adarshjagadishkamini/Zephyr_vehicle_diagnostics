#include "traffic_manager.h"
#include "v2x_router.h"

#define MAX_TRAFFIC_LIGHTS 32
#define MAX_INTERSECTIONS 16

struct traffic_light {
    uint32_t id;
    uint8_t state;
    uint32_t timing;
    float distance;
    struct {
        float latitude;
        float longitude;
    } location;
};

static struct traffic_light traffic_lights[MAX_TRAFFIC_LIGHTS];
static uint8_t num_lights = 0;

void traffic_manager_init(void) {
    memset(traffic_lights, 0, sizeof(traffic_lights));
    register_v2x_handler(V2X_MSG_TRAFFIC, handle_traffic_message);
}

void handle_traffic_light_update(struct traffic_light *light) {
    // Update local traffic light state
    for (int i = 0; i < num_lights; i++) {
        if (traffic_lights[i].id == light->id) {
            memcpy(&traffic_lights[i], light, sizeof(struct traffic_light));
            calculate_approach_speed(light);
            broadcast_traffic_state(light);
            break;
        }
    }
}
