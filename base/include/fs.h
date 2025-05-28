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

extern void fs_init(void);
extern ssize_t fs_read_line(struct fs_file_t *file, char *buf, size_t max_len);

#endif