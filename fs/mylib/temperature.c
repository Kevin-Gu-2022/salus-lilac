#include "sensor_interface.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/mpsc_lockfree.h>

void temp_producer_thread(void *, void *, void *) {

    struct sensor_value temp;
    const struct device *temp_dev= DEVICE_DT_GET_ONE(st_hts221);

    if (!device_is_ready(temp_dev)) {
		printk("sensor: device not ready.\n");
		return;
	}

    while (1) {

        k_sem_take(&temp_sem, K_FOREVER);

        if (sensor_sample_fetch(temp_dev) < 0) {
            printf("Sensor sample update error\n");
            return;
        }

        if (sensor_channel_get(temp_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp) < 0) {
            printf("Cannot read HTS221 temperature channel\n");
            return;
        }

        packets[0].did = 0;
        packets[0].value = sensor_value_to_double(&temp);
        mpsc_push(&sensor_queue, &packets[0].node);
        k_sem_give(&consumer_sem);
    }
}

K_THREAD_DEFINE(temp_prod_id, STACKSIZE, temp_producer_thread, NULL, NULL, NULL, 5, 0, 0);
