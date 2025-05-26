/*
* @file     user.h
* @brief    User Configuration Library
* @author   Lachlan Chun, 47484874
*/

#ifndef USER_H
#define USER_H

#include <zephyr/kernel.h>
#include <zephyr/sys/slist.h>
#include <zephyr/shell/shell.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

typedef struct {
    char *alias;
    char *mac;
    char *passcode;
    sys_snode_t node;
} user_config_t;

// Status codes
#define USER_SUCCESS 0
#define USER_NOT_FOUND -1
#define USER_ALREADY_EXISTS -2
#define USER_MAC_ALREADY_EXISTS -3
#define USER_MAC_INVALID -4
#define USER_PASSCODE_INVALID -5
#define USER_MEMORY_ERROR -6

#define MAC_ADDRESS_LENGTH 18
#define PASSCODE_LENGTH 5

extern sys_slist_t user_config_list;
extern struct k_mutex user_config_list_mutex;
extern struct k_sem user_list_update_sem;

// Function declarations
int user_add(const char *alias, const char *mac, const char *passcode);
int user_remove(const char *alias);
void user_view(const struct shell *shell, const char *alias);
void user_init(void);

#endif