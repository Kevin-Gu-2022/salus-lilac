#include "sensor_interface.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/mpsc_lockfree.h>

void sensor_consumer_thread(void *, void *, void *) {

    struct mpsc_node *sensor_node;
    struct sensor_data_packet *packet = NULL;

    while (1) {

        k_sem_take(&consumer_sem, K_FOREVER);
        while ((sensor_node = mpsc_pop(&sensor_queue)) != NULL) {
            packet = CONTAINER_OF(sensor_node, struct sensor_data_packet, node);
            switch (packet->did) {
                case SENSOR_TEMP:
                case SENSOR_HUMIDITY:
                case SENSOR_BAROMETER:
                case SENSOR_MAGNETOMETER:
                    received_packets[packet->did] = *packet;
                    k_sem_give(sensor_info[packet->did].ready_sem);  // Signal AFTER storing
                    break;
                default:
                    break;
            }
        }    
    }
}

K_THREAD_DEFINE(sensor_con_id, STACKSIZE, sensor_consumer_thread, NULL, NULL, NULL, 5, 0, 0);

