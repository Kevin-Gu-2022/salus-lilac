/*
* @file     servo.h
* @brief    Servo Motor Library
* @author   Xinlin Zhong, 48018061
*/

#ifndef SERVO_H
#define SERVO_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>

extern void servo_init(void);
extern void servo_toggle(void);

#endif