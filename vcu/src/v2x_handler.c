#include "v2x_handler.h"
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#define MAX_V2V_PAYLOAD 240
#define V2V_SERVICE_UUID BT_UUID_DECLARE_16(0xFF00)
#define V2V_DATA_UUID    BT_UUID_DECLARE_16(0xFF01)

static struct bt_le_adv_param adv_param = BT_LE_ADV_PARAM(
    BT_LE_ADV_OPT_CONNECTABLE,
    BT_GAP_ADV_FAST_INT_MIN_2,
    BT_GAP_ADV_FAST_INT_MAX_2,
    NULL);

// V2V GATT service definition
BT_GATT_SERVICE_DEFINE(v2v_svc,
    BT_GATT_PRIMARY_SERVICE(V2V_SERVICE_UUID),
    BT_GATT_CHARACTERISTIC(V2V_DATA_UUID,
                          BT_GATT_CHRC_WRITE_WITHOUT_RESP | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_WRITE,
                          NULL, NULL, NULL),
);

static struct bt_le_ext_adv *adv;

void v2x_init(void) {
    int err;

    // Initialize extended advertising
    err = bt_le_ext_adv_create(&adv_param, NULL, &adv);
    if (err) {
        return;
    }

    // Start advertising
    err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_PARAM);
    if (err) {
        return;
    }
}

int broadcast_v2v_data(uint8_t type, const uint8_t *data, uint16_t len) {
    uint8_t v2v_packet[MAX_V2V_PAYLOAD];
    if (len + 1 > MAX_V2V_PAYLOAD) {
        return -EINVAL;
    }

    // Construct V2V packet
    v2v_packet[0] = type;
    memcpy(&v2v_packet[1], data, len);

    // Broadcast using BLE advertising data
    struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA, 
                     (v2v_packet[0] & 0xFF),
                     (v2v_packet[1] & 0xFF)),
        BT_DATA(BT_DATA_SVC_DATA16, &v2v_packet[2], len - 2),
    };

    return bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);
}
