#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/can.h>
#include "sensor_common.h"
#include "can_ids.h"

#define ADC_NODE DT_NODELABEL(adc0)
#define ADC_CHANNEL 0
#define ADC_RESOLUTION 12
#define ADC_GAIN ADC_GAIN_1
#define ADC_REFERENCE ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME_DEFAULT

static const struct device *adc_dev;
static const struct device *can_dev;
static uint16_t adc_raw;

static const struct adc_channel_cfg channel_cfg = {
    .gain = ADC_GAIN,
    .reference = ADC_REFERENCE,
    .acquisition_time = ADC_ACQUISITION_TIME,
    .channel_id = ADC_CHANNEL,
};

static void brake_thread(void *arg1, void *arg2, void *arg3) {
    struct adc_sequence sequence = {
        .channels = BIT(ADC_CHANNEL),
        .buffer = &adc_raw,
        .buffer_size = sizeof(adc_raw),
        .resolution = ADC_RESOLUTION,
    };

    while (1) {
        adc_sequence_init_dt(&sequence);
        adc_read(adc_dev, &sequence);

        uint8_t can_data[BRAKE_MSG_LEN];
        can_data[0] = adc_raw >> 8;
        can_data[1] = adc_raw & 0xFF;
        
        send_sensor_data(can_dev, CAN_ID_BRAKE, can_data, BRAKE_MSG_LEN);
        
        k_sleep(K_MSEC(50));  // 20Hz sampling rate
    }
}

void main(void) {
    adc_dev = DEVICE_DT_GET(ADC_NODE);
    can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

    if (!device_is_ready(adc_dev) || !device_is_ready(can_dev)) {
        return;
    }

    adc_channel_setup(adc_dev, &channel_cfg);

    k_thread_create(&brake_thread_data, brake_stack,
                   SENSOR_THREAD_STACK_SIZE,
                   brake_thread, NULL, NULL, NULL,
                   SENSOR_THREAD_PRIORITY, 0, K_NO_WAIT);
}
