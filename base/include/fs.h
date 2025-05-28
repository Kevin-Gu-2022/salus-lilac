/*
* @file     fs.h
* @brief    File System Library
* @author   Lachlan Chun, 47484874
*/

#ifndef FS_H
#define FS_H

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <zephyr/data/json.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "user.h"
#include "sensor.h"

#define STACK_SIZE 4096
#define THREAD_PRIORITY 1

extern void fs_init(void);

#endif