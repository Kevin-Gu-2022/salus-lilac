/*
* @file     sensor.c
* @brief    Sensor Configuration Library
* @author   Lachlan Chun, 47484874
*/

#include "sensor.h"

struct k_sem sensor_threshold_update_sem;

static sensor_threshold_t thresholds = {
    .ultra_threshold = "0.080\0",
    .mag_threshold = "0.050\0"
};

const sensor_threshold_t *sensor_get_thresholds(void) {
    return &thresholds;
}

void sensor_set_thresholds(const sensor_threshold_t *new_thresholds) {
    if (new_thresholds) {
        thresholds = *new_thresholds;  // copy all threshold values
        k_sem_give(&sensor_threshold_update_sem);
    }
}

void sensor_set_ultrasonic_threshold(double value) {
    snprintf(thresholds.ultra_threshold, THRESHOLD_LENGTH, "%.3f", value);
    k_sem_give(&sensor_threshold_update_sem);
}

void sensor_set_magnetometer_threshold(double value) {
    snprintf(thresholds.mag_threshold, THRESHOLD_LENGTH, "%.3f", value);
    k_sem_give(&sensor_threshold_update_sem);
}

double sensor_get_ultrasonic_threshold(void) {
    return strtod(thresholds.ultra_threshold, NULL);
}

double sensor_get_magnetometer_threshold(void) {
    return strtod(thresholds.mag_threshold, NULL);
}

void sensor_threshold_init(void) {
    k_sem_init(&sensor_threshold_update_sem, 0, 1);
}