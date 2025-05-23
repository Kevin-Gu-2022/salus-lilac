#include "sensor_interface.h"

K_SEM_DEFINE(temp_sem, 0, 1);
K_SEM_DEFINE(hum_sem, 0, 1);
K_SEM_DEFINE(pres_sem, 0, 1);
K_SEM_DEFINE(magn_sem, 0, 1);

K_SEM_DEFINE(temp_ready_sem, 0, 1);
K_SEM_DEFINE(hum_ready_sem, 0, 1);
K_SEM_DEFINE(pres_ready_sem, 0, 1);
K_SEM_DEFINE(magn_ready_sem, 0, 1);

K_SEM_DEFINE(consumer_sem, 0, 1);

struct mpsc sensor_queue = MPSC_INIT(sensor_queue);

struct sensor_data_packet packets[PACKET_POOL_SIZE];
struct sensor_data_packet received_packets[SENSOR_COUNT];

sensor_info_t sensor_info[] = {
    [SENSOR_TEMP]           = { &temp_sem, &temp_ready_sem, "🌡️ Temperature :", "°C" },
    [SENSOR_HUMIDITY]       = { &hum_sem,  &hum_ready_sem, "💧Humidity    :", "%" },
    [SENSOR_BAROMETER]       = { &pres_sem, &pres_ready_sem, "⛅Pressure    :", "hPa" },
    [SENSOR_MAGNETOMETER] = { &magn_sem, &magn_ready_sem, "🧭Mag", "µT" }
};




