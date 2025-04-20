#include "mqtt_handler.h"
#include <zephyr/net/socket.h>
#include <zephyr/random/rand32.h>

#define MQTT_CLIENTID "vcu-%08x"
#define MQTT_BROKER_HOSTNAME "broker.emq.io"
#define MQTT_BROKER_PORT 1883

static uint8_t rx_buffer[1024];
static uint8_t tx_buffer[1024];

void mqtt_client_init(struct mqtt_client *client) {
    struct sockaddr_in broker;
    char clientid[32];

    snprintf(clientid, sizeof(clientid), MQTT_CLIENTID,
             sys_rand32_get());

    broker.sin_family = AF_INET;
    broker.sin_port = htons(MQTT_BROKER_PORT);
    net_addr_pton(AF_INET, MQTT_BROKER_HOSTNAME, &broker.sin_addr);

    mqtt_client_init(client);

    client->broker = &broker;
    client->evt_cb = mqtt_evt_handler;
    client->client_id.utf8 = clientid;
    client->client_id.size = strlen(clientid);
    client->password = NULL;
    client->user_name = NULL;
    client->protocol_version = MQTT_VERSION_3_1_1;
    client->rx_buf = rx_buffer;
    client->rx_buf_size = sizeof(rx_buffer);
    client->tx_buf = tx_buffer;
    client->tx_buf_size = sizeof(tx_buffer);
}

void publish_sensor_data(const char *topic, float value) {
    char payload[32];
    struct mqtt_publish_param param;

    snprintf(payload, sizeof(payload), "%.2f", value);

    param.message.topic.qos = 1;
    param.message.topic.topic.utf8 = topic;
    param.message.topic.topic.size = strlen(topic);
    param.message.payload.data = payload;
    param.message.payload.len = strlen(payload);
    param.message_id = sys_rand32_get();
    param.dup_flag = 0;
    param.retain_flag = 0;

    mqtt_publish(&client, &param);
}

void publish_to_topic(const char *topic, const char *payload)
{
    struct mqtt_publish_param param = {
        .message.topic.qos = 1,
        .message.topic.topic.utf8 = topic,
        .message.topic.topic.size = strlen(topic),
        .message.payload.data = payload,
        .message.payload.len = strlen(payload),
        .message_id = sys_rand32_get(),
        .dup_flag = 0,
        .retain_flag = 0
    };

    mqtt_publish(mqtt_client, &param);
}

void subscribe_to_topics(void)
{
    static struct mqtt_topic_list topics = {
        .list = {
            { .topic = { .utf8 = TOPIC_V2I } },
            { .topic = { .utf8 = TOPIC_TRAFFIC_UPDATE } },
            { .topic = { .utf8 = TOPIC_HAZARD_NOTIFICATION } }
        },
        .count = 3
    };
    
    mqtt_subscribe(mqtt_client, &topics);
}
