#include "sensor_interface.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/mpsc_lockfree.h>

void pressure_producer_thread(void *, void *, void *) {

    struct sensor_value pres;
    const struct device *const pressure_dev = DEVICE_DT_GET_ONE(st_lps22hb_press);

    if (!device_is_ready(pressure_dev)) {
        printf("Device %s is not ready\n", pressure_dev->name);
        return;
    }

    while (1) {

        k_sem_take(&pres_sem, K_FOREVER);

        if (sensor_sample_fetch(pressure_dev) < 0) {
            printf("Sensor sample update error\n");
            return;
        }

        if (sensor_channel_get(pressure_dev, SENSOR_CHAN_PRESS, &pres) < 0) {
            printf("Cannot read LPS22HB pressure channel\n");
            return;
        }

        packets[2].did = 2;
        packets[2].value = sensor_value_to_double(&pres);
        mpsc_push(&sensor_queue, &packets[2].node);
        k_sem_give(&consumer_sem);
    }
}

K_THREAD_DEFINE(pressure_prod_id, STACKSIZE, pressure_producer_thread, NULL, NULL, NULL, 5, 0, 0);

