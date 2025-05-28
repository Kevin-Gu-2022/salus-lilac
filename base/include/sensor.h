/*
* @file     sensor.h
* @brief    Sensor Configuration Library
* @author   Lachlan Chun, 47484874
*/

#ifndef SENSOR_H
#define SENSOR_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define THRESHOLD_LENGTH        16
#define MAG_BASELINE_X          -0.845
#define MAG_BASELINE_Y          0.505
#define MAG_BASELINE_Z          0.200
#define MAG_BASELINE_SQ  (MAG_BASELINE_X * MAG_BASELINE_X + MAG_BASELINE_Y * MAG_BASELINE_Y + MAG_BASELINE_Z * MAG_BASELINE_Z)

typedef struct {
    char mag_threshold[THRESHOLD_LENGTH];
    char ultra_threshold[THRESHOLD_LENGTH];
} sensor_threshold_t;

extern const sensor_threshold_t *sensor_get_thresholds(void);
extern void sensor_set_thresholds(const sensor_threshold_t *new_thresholds);
extern void sensor_set_ultrasonic_threshold(double value);
extern void sensor_set_magnetometer_threshold(double value);
extern double sensor_get_ultrasonic_threshold(void);
extern double sensor_get_magnetometer_threshold(void);
extern void sensor_threshold_init(void);

extern struct k_sem sensor_threshold_update_sem;

#endif