#ifndef SENSOR_INTERFACE_H_
#define SENSOR_INTERFACE_H_

#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/mpsc_lockfree.h>

#define SENSOR_COUNT 16
#define PACKET_POOL_SIZE 4 
#define STACKSIZE 1024
#define NUM_OF_ELEMENTS 8  // 8 elements, must be power of 2

extern struct k_sem temp_sem;
extern struct k_sem hum_sem;
extern struct k_sem pres_sem;
extern struct k_sem magn_sem;

extern struct k_sem temp_ready_sem;
extern struct k_sem hum_ready_sem;
extern struct k_sem pres_ready_sem;
extern struct k_sem magn_ready_sem;

extern struct k_sem consumer_sem;

extern struct mpsc sensor_queue;

struct sensor_data_packet {
    struct mpsc_node node;
    uint8_t did;
    union {
        double value;          // For temp, humidity, pressure
        double magn[3];        // For magnetometer
    };
};

extern struct mpsc sensor_queue;
extern struct sensor_data_packet packets[PACKET_POOL_SIZE];
extern struct sensor_data_packet received_packets[SENSOR_COUNT];

typedef struct {
    struct k_sem *sem;
    struct k_sem *ready_sem;
    const char *label;
    const char *unit;
} sensor_info_t;

typedef enum {
    SENSOR_TEMP = 0,
    SENSOR_HUMIDITY = 1,
    SENSOR_BAROMETER = 2,
    SENSOR_MAGNETOMETER = 4,
    SENSOR_ALL = 15
} sensor_id_t;

extern sensor_info_t sensor_info[];


#endif