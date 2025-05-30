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
#include <stdbool.h>
#include <string.h>

#include <zephyr/logging/log.h>

#define STACKSIZE           4096
#define DEVICE_NAME		    CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN		(sizeof(DEVICE_NAME) - 1)

extern int bluetooth_init(void);
extern void bluetooth_write(const char *msg, size_t len);
extern uint32_t get_synchronized_time_ms();

#endif