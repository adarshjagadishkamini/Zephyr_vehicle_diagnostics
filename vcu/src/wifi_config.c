#include "wifi_config.h"
#include "secure_storage.h"

static struct wifi_credentials {
    char ssid[MAX_SSID_LEN];
    char password[MAX_PSK_LEN];
} credentials;

void wifi_init(void) {
    memset(&credentials, 0, sizeof(credentials));
    load_wifi_credentials();
}

int wifi_connect(const char *ssid, const char *password) {
    struct wifi_connect_req_params params = {
        .ssid = ssid,
        .ssid_length = strlen(ssid),
        .psk = password,
        .psk_length = strlen(password),
        .channel = WIFI_CHANNEL_ANY,
        .security = WIFI_SECURITY_TYPE_PSK
    };
    
    return net_mgmt(NET_REQUEST_WIFI_CONNECT, wifi_dev, &params, sizeof(params));
}

void store_wifi_credentials(const char *ssid, const char *password) {
    if (ssid) {
        strncpy(credentials.ssid, ssid, MAX_SSID_LEN - 1);
    }
    if (password) {
        strncpy(credentials.password, password, MAX_PSK_LEN - 1);
        secure_storage_write("wifi_creds", &credentials, sizeof(credentials));
    }
}
