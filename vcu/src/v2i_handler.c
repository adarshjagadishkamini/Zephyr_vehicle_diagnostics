#include "v2i_handler.h"
#include "mqtt_handler.h"
#include <zephyr/data/json.h>

static struct v2i_context {
    bool traffic_light_state;
    uint8_t road_condition;
    uint16_t traffic_flow;
} v2i_ctx;

void v2i_init(void) {
    memset(&v2i_ctx, 0, sizeof(v2i_ctx));
    subscribe_to_v2i_topics();
}

int send_v2i_data(uint8_t type, const uint8_t *data, uint16_t len) {
    char json_buffer[256];
    switch(type) {
        case V2I_TRAFFIC_LIGHT:
            snprintf(json_buffer, sizeof(json_buffer), 
                    "{\"type\":\"traffic_light\",\"state\":%d}", data[0]);
            break;
        case V2I_ROAD_CONDITION:
            snprintf(json_buffer, sizeof(json_buffer), 
                    "{\"type\":\"road_condition\",\"condition\":%d}", data[0]);
            break;
        case V2I_TRAFFIC_FLOW:
            snprintf(json_buffer, sizeof(json_buffer), 
                    "{\"type\":\"traffic_flow\",\"density\":%d}", 
                    (data[0] << 8) | data[1]);
            break;
    }
    return publish_to_topic(TOPIC_V2I, json_buffer);
}

static void handle_traffic_light_data(const struct json_obj_token *token) {
    uint8_t light_state;
    float distance;
    
    // Parse traffic light data
    json_decode_uint8(token, "state", &light_state);
    json_decode_float(token, "distance", &distance);
    
    // Update local context
    v2i_ctx.traffic_light_state = light_state;
    
    // Handle traffic light state
    switch (light_state) {
        case LIGHT_RED:
            if (distance < CRITICAL_DISTANCE) {
                trigger_brake_warning();
            }
            break;
            
        case LIGHT_YELLOW:
            calculate_stopping_distance(distance);
            break;
    }
}

static void handle_road_condition_data(const struct json_obj_token *token) {
    uint8_t condition;
    float friction;
    
    json_decode_uint8(token, "condition", &condition);
    json_decode_float(token, "friction", &friction);
    
    // Update local context
    v2i_ctx.road_condition = condition;
    
    // Adjust vehicle parameters based on road condition
    switch (condition) {
        case ROAD_ICY:
            adjust_traction_control(friction);
            broadcast_road_hazard();
            break;
            
        case ROAD_WET:
            adjust_abs_parameters();
            break;
    }
}

static void handle_traffic_flow_data(const struct json_obj_token *token) {
    uint16_t density;
    float avg_speed;
    
    json_decode_uint16(token, "density", &density);
    json_decode_float(token, "avg_speed", &avg_speed);
    
    // Update traffic flow context
    v2i_ctx.traffic_flow = density;
    
    // Optimize route based on traffic flow
    if (density > HIGH_TRAFFIC_THRESHOLD) {
        suggest_alternate_route();
        notify_traffic_congestion();
    }
}

void process_v2i_message(const uint8_t *data, uint16_t len) {
    struct json_obj_token token;
    
    if (json_obj_parse(data, len, &token) == 0) {
        char type[32];
        json_decode_string(&token, "type", type, sizeof(type));
        
        if (strcmp(type, "traffic_light") == 0) {
            handle_traffic_light_data(&token);
        } else if (strcmp(type, "road_condition") == 0) {
            handle_road_condition_data(&token);
        } else if (strcmp(type, "traffic_flow") == 0) {
            handle_traffic_flow_data(&token);
        }
        
        // Update traffic management system
        update_traffic_management_system(&token);
    }
}
