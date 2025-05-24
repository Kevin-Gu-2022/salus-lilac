/*
* @file     event.h
* @brief    Event Detection and Classification
* @author   Lachlan Chun, 47484874
*/

#ifndef EVENT_H
#define EVENT_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "user.h"
#include "bluetooth.h"
#include "servo.h"

#define STACK_SIZE 4096
#define THREAD_PRIORITY 1
// #define MSGQ_SIZE 10
#define PASSCODE_ATTEMPTS 3

#endif