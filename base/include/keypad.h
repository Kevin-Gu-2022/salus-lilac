/*
* @file     keypad.h
* @brief    Keypad Library
* @author   Xinlin Zhong, 48018061
*/

#ifndef KEYPAD_H
#define KEYPAD_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <stdbool.h>

#define ROW_COUNT 4
#define COL_COUNT 4

extern void keypad_init(void);
extern char keypad_scan(void);

#endif