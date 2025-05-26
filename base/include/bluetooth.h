/*
* @file     bluetooth.h
* @brief    Bluetooth Library
* @author   Xinlin Zhong, 48018061
*/

#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/sys/slist.h>
#include <stdbool.h>
#include <string.h>
#include "user.h"

#define STACKSIZE           4096
#define DEVICE_NAME		    CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN		(sizeof(DEVICE_NAME) - 1)
#define MSGQ_SIZE           10

extern struct k_msgq user_mac_msgq;
extern struct k_msgq user_passcode_msgq;
extern struct k_sem user_disconnect_sem;

extern int bluetooth_init(void);
extern int bluetooth_advertise(void);
extern void bluetooth_write(const char *msg, size_t len);

#endif