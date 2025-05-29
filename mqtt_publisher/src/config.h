/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

// #define ZEPHYR_ADDR		"192.168.1.118"
// #define SERVER_ADDR		"192.168.1.118"

// #define ZEPHYR_ADDR		"172.20.10.2"
// #define SERVER_ADDR		"172.20.10.2"

// #define ZEPHYR_ADDR		"10.151.70.195"
// #define SERVER_ADDR		"10.151.70.195"

#define ZEPHYR_ADDR		"192.168.1.113"
#define SERVER_ADDR		"192.168.1.113"

#define SERVER_PORT		1883

// #if defined(CONFIG_SOCKS)
// #define SOCKS5_PROXY_ADDR	SERVER_ADDR
// #define SOCKS5_PROXY_PORT	1080
// #endif

// #ifdef CONFIG_MQTT_LIB_TLS
// #ifdef CONFIG_MQTT_LIB_WEBSOCKET
// #define SERVER_PORT		9001
// #else
// #define SERVER_PORT		8883
// #endif /* CONFIG_MQTT_LIB_WEBSOCKET */
// #else
// #ifdef CONFIG_MQTT_LIB_WEBSOCKET
// #define SERVER_PORT		9001
// #else
// #define SERVER_PORT		1883
// #endif /* CONFIG_MQTT_LIB_WEBSOCKET */
// #endif

#define APP_CONNECT_TIMEOUT_MS	2000
#define APP_SLEEP_MSECS		500

#define APP_CONNECT_TRIES	10

#define APP_MQTT_BUFFER_SIZE	128

#define MQTT_CLIENTID		"zephyr_publisher"

/* Set the following to 1 to enable the Bluemix topic format */
#define APP_BLUEMIX_TOPIC	0

/* These are the parameters for the Bluemix topic format */
#if APP_BLUEMIX_TOPIC
#define BLUEMIX_DEVTYPE		"sensor"
#define BLUEMIX_DEVID		"carbon"
#define BLUEMIX_EVENT		"status"
#define BLUEMIX_FORMAT		"json"
#endif

#endif
