/*
* @file     event.h
* @brief    Event Detection and Classification
* @author   Lachlan Chun, 47484874
*/

#ifndef EVENT_H
#define EVENT_H

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/slist.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "user.h"
#include "servo.h"
#include "keypad.h"
#include "sensor.h"
#include "blockchain.h"

#define STACK_SIZE             4096
#define THREAD_PRIORITY        1
#define PASSCODE_ATTEMPTS      3
#define DEVICE_NAME		       CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN		   (sizeof(DEVICE_NAME) - 1)
#define MSGQ_SIZE              10
#define MOBILE_CONNECT_TIMEOUT K_SECONDS(10)
#define DISPLAY_BUFFER_SIZE 7

typedef enum {
    STATE_IDLE,
    STATE_SENSOR_CONNECT,
    STATE_SENSOR_DATA,
    STATE_SENSOR_DISCONNECT,
    STATE_MOBILE_CONNECT,
    STATE_MOBILE_DATA,
    STATE_MOBILE_DISCONNECT,
    STATE_TAMPERING,
	STATE_PRESENCE,
	STATE_PASSCODE,
	STATE_FAIL,
	STATE_SUCCESS,
	STATE_BLOCKCHAIN,
    STATE_MAX
} system_state_t;

typedef enum {
    EVENT_NONE,
    EVENT_PRESENCE,
    EVENT_TAMPERING,
    EVENT_FAIL,
    EVENT_SUCCESS
} event_type_t;

extern const char *state_names[STATE_MAX];

extern struct k_msgq user_mac_msgq;
extern struct k_msgq user_passcode_msgq;
extern struct k_sem user_disconnect_sem;
extern struct k_sem sensor_connect_sem;

extern void bluetooth_init(void);
extern void bluetooth_scan(void);
extern void bluetooth_write(const char *string);


#endif