/*
* @file     main.c
* @brief    Smart Entry System Project - Sensors Node
* @author   Lachlan Chun, 47484874
*/

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/sensor.h>
#include "bluetooth.h"
#include "time_sync.h"

const struct device *ultrasonic_dev;
const struct device *magnetometer_dev;

void magnetometer_init(void) {
    magnetometer_dev = DEVICE_DT_GET_ONE(st_lis3mdl_magn);

    if (!device_is_ready(magnetometer_dev)) {
        printk("Magnetometer device not ready\n");
        return;
    }
}

void ultrasonic_init(void) {
    ultrasonic_dev = DEVICE_DT_GET(DT_ALIAS(ultrasonic));

    if (ultrasonic_dev == NULL) {
    printk("Failed to get ultrasonic sensor binding");
        return;
    }
}

void magnetometer_measure(const struct device *dev) {
    struct sensor_value magn[3];
    
    if (sensor_sample_fetch(dev) < 0) {
        printf("Magnetometer sample update error\n");
        return;
    }

    if (sensor_channel_get(dev, SENSOR_CHAN_MAGN_XYZ, magn) < 0) {
        printk("Magnetometer channel get failed\n");
        return;
    }

    char msg_buf[21];

    double x = sensor_value_to_double(&magn[0]);
    double y = sensor_value_to_double(&magn[1]);
    double z = sensor_value_to_double(&magn[2]);

    snprintf(msg_buf, sizeof(msg_buf), "%.3f,%.3f,%.3f", x, y, z);

    bluetooth_write(msg_buf, strlen(msg_buf));
    printk("%s\n", msg_buf);
}

void ultrasonic_measure(const struct device *dev) {
    int ret;
    struct sensor_value distance;

    ret = sensor_sample_fetch_chan(dev, SENSOR_CHAN_ALL);
    switch (ret) {
        case 0:
            ret = sensor_channel_get(dev, SENSOR_CHAN_DISTANCE, &distance);
            if (ret) {
                printk("sensor_channel_get failed ret %d", ret);
                return;
            }

            int integer_part = distance.val1;
            int fractional_part = distance.val2 / 1000;

            char msg_buf[30];
            snprintf(msg_buf, sizeof(msg_buf), "%d.%03d", integer_part, fractional_part);
            bluetooth_write(msg_buf, strlen(msg_buf));
            printk("%s\n", msg_buf);
            break;
        case -EIO:
            printk("Could not read from ultrasonic device\n");
            break;
        default:
            printk("Error when reading ultrasonic device\n");
            break;
    }
}

// Main.
int main(void) {
    magnetometer_init();
    ultrasonic_init();
    
    bluetooth_init();
    k_msleep(1000);

    while (1) {
        ultrasonic_measure(ultrasonic_dev);
        k_msleep(250);
        magnetometer_measure(magnetometer_dev);
        k_msleep(250);

        update_time_sync_regression();

    }
}