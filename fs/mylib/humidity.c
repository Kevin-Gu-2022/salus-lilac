#include "sensor_interface.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/mpsc_lockfree.h>

void hum_producer_thread(void *, void *, void *) {

    struct sensor_value hum;
    const struct device *hum_dev= DEVICE_DT_GET_ONE(st_hts221);

    if (!device_is_ready(hum_dev)) {
		printk("sensor: device not ready.\n");
		return;
	}

    while (1) {

        k_sem_take(&hum_sem, K_FOREVER);

        if (sensor_sample_fetch(hum_dev) < 0) {
            printf("Sensor sample update error\n");
            return;
        }

        if (sensor_channel_get(hum_dev, SENSOR_CHAN_HUMIDITY, &hum) < 0) {
            printf("Cannot read HTS221 humidity channel\n");
            return;
        }

        packets[1].did = 1;
        packets[1].value = sensor_value_to_double(&hum);
        mpsc_push(&sensor_queue, &packets[1].node);
        k_sem_give(&consumer_sem);
    }
}

K_THREAD_DEFINE(hum_prod_id, STACKSIZE, hum_producer_thread, NULL, NULL, NULL, 5, 0, 0);


