#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <zephyr/net/mqtt.h>

void mqtt_client_init(struct mqtt_client *client);
void publish_sensor_data(const char *topic, float value);
void publish_gps_data(const char *topic, float lat, float lon);
void subscribe_to_topics(void);
void mqtt_connect_work_handler(struct k_work *work);

#endif /* MQTT_HANDLER_H */
