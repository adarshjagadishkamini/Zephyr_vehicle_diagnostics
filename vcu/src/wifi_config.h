#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <zephyr/net/wifi_mgmt.h>

#define MAX_SSID_LEN 32
#define MAX_PSK_LEN 64

void wifi_init(void);
int wifi_connect(const char *ssid, const char *password);
void store_wifi_credentials(const char *ssid, const char *password);

#endif /* WIFI_CONFIG_H */
