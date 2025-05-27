/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#define MY_WIFI_SSID        "Kevin's Phone"
#define MY_WIFI_PASSWORD    "password"

// #define ZEPHYR_ADDR		"192.168.1.101"
#define SERVER_ADDR	"broker.hivemq.com"
#define SERVER_PORT		1883

#define APP_CONNECT_TIMEOUT_MS	15000
#define APP_SLEEP_MSECS		500

#define APP_CONNECT_TRIES	10

#define APP_MQTT_BUFFER_SIZE	128

#define APP_KEEPALIVE_TIME_SECONDS 60

// #define MQTT_CLIENTID		"zephyr_publishe_0987654"

/* Set the following to 1 to enable the Bluemix topic format */
#define APP_BLUEMIX_TOPIC	0

#endif
