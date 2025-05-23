#include "sensor_interface.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/mpsc_lockfree.h>

void magn_producer_thread(void *, void *, void *) {

    struct sensor_value magn[3];
    const struct device *magn_dev = DEVICE_DT_GET_ONE(st_lis3mdl_magn);

    if (!device_is_ready(magn_dev)) {
        printk("Magnetometer device not ready\n");
        return;
    }

    while (1) {

        k_sem_take(&magn_sem, K_FOREVER);

        if (sensor_sample_fetch(magn_dev) < 0) {
            printf("Sensor sample update error\n");
            return;
        }

        if (sensor_channel_get(magn_dev, SENSOR_CHAN_MAGN_XYZ, magn) < 0) {
            printk("Magnetometer channel get failed\n");
            return;
        }
   
        packets[3].did = 4;
        packets[3].magn[0] = sensor_value_to_double(&magn[0]);
        packets[3].magn[1] = sensor_value_to_double(&magn[1]);
        packets[3].magn[2] = sensor_value_to_double(&magn[2]);
        mpsc_push(&sensor_queue, &packets[3].node);
        k_sem_give(&consumer_sem);
    }
}

K_THREAD_DEFINE(magn_prod_id, STACKSIZE, magn_producer_thread, NULL, NULL, NULL, 5, 0, 0);

