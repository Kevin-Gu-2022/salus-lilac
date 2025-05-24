/*
* @file     user.c
* @brief    User Configuration Library
* @author   Lachlan Chun, 47484874
*/

#include "user.h"

// Singly linked list to hold user nodes, and mutex and semaphore for thread safety
sys_slist_t user_config_list;
struct k_mutex user_config_list_mutex;
struct k_sem user_list_update_sem;

// Checks if the given MAC address is valid
bool user_valid_max(const char *mac) {
    unsigned int bytes[6];
    // sscanf will try to parse 6 hex values separated by colons
    int parsed = sscanf(mac, "%2x:%2x:%2x:%2x:%2x:%2x",
                        &bytes[0], &bytes[1], &bytes[2],
                        &bytes[3], &bytes[4], &bytes[5]);

    // Check if 6 values were successfully parsed
    if (parsed != 6)
        return false;

    // Optionally: Check string length is exactly 17 (for strict match)
    for (int i = 0; mac[i] != '\0'; i++) {
        if (i >= 17) return false;
    }

    return true;
}

// Checks if the given passcode is valid
bool user_valid_passcode(const char *passcode) {
    if (!passcode || strlen(passcode) != 4)
        return false;

    for (int i = 0; i < 4; i++) {
        if (!isdigit((unsigned char)passcode[i]))
            return false;
    }

    return true;
}

// Add a user to the linked list if they exist in predefined users
int user_add(const char *alias, const char *mac, const char *passcode) {
    if (!user_valid_max(mac)) {
        return USER_MAC_INVALID;
    }

    if (!user_valid_passcode(passcode)) {
        return USER_PASSCODE_INVALID;
    }
    
    k_mutex_lock(&user_config_list_mutex, K_FOREVER);

    sys_snode_t *snode;
    SYS_SLIST_FOR_EACH_NODE(&user_config_list, snode) {
        user_config_t *entry = CONTAINER_OF(snode, user_config_t, node);
        if (strcmp(entry->alias, alias) == 0) {
            k_mutex_unlock(&user_config_list_mutex);
            return USER_ALREADY_EXISTS;
        }
        if (strcmp(entry->mac, mac) == 0) {
            k_mutex_unlock(&user_config_list_mutex);
            return USER_MAC_ALREADY_EXISTS;
        }
    }

    user_config_t *new_user = k_malloc(sizeof(user_config_t));
    if (!new_user) {
        k_mutex_unlock(&user_config_list_mutex);
        return USER_MEMORY_ERROR;
    }

    // Copy strings to dynamically allocated memory
    new_user->alias = k_malloc(strlen(alias) + 1);
    new_user->mac = k_malloc(strlen(mac) + 1);
    new_user->passcode = k_malloc(strlen(passcode) + 1);

    if (!new_user->alias || !new_user->mac || !new_user->passcode) {
        k_free(new_user->alias);
        k_free(new_user->mac);
        k_free(new_user->passcode);
        k_free(new_user);
        k_mutex_unlock(&user_config_list_mutex);
        return USER_MEMORY_ERROR;
    }

    strcpy((char *)new_user->alias, alias);
    strcpy((char *)new_user->mac, mac);
    strcpy((char *)new_user->passcode, passcode);

    sys_slist_append(&user_config_list, &new_user->node);
    k_mutex_unlock(&user_config_list_mutex);
    k_sem_give(&user_list_update_sem);
    
    return USER_SUCCESS;
}

// Remove a user by alias from the linked lists
int user_remove(const char *alias) {
    k_mutex_lock(&user_config_list_mutex, K_FOREVER);

    sys_snode_t *prev = NULL;
    sys_snode_t *snode = sys_slist_peek_head(&user_config_list);

    while (snode != NULL) {
        user_config_t *entry = CONTAINER_OF(snode, user_config_t, node);
        if (strcmp(entry->alias, alias) == 0) {
            sys_slist_remove(&user_config_list, prev, snode);
            k_free(entry);
            k_mutex_unlock(&user_config_list_mutex);
            k_sem_give(&user_list_update_sem);
            return USER_SUCCESS;
        }
        prev = snode;
        snode = sys_slist_peek_next(snode);
    }

    k_mutex_unlock(&user_config_list_mutex);
    return USER_NOT_FOUND;
}

// View details of a specific user or all users
void user_view(const struct shell *shell, const char *alias) {
    k_mutex_lock(&user_config_list_mutex, K_FOREVER);

    if (strcmp(alias, "-a") == 0) {
        sys_snode_t *snode;
        SYS_SLIST_FOR_EACH_NODE(&user_config_list, snode) {
            user_config_t *entry = CONTAINER_OF(snode, user_config_t, node);
            shell_print(shell, "{Alias: %s, MAC Address: %s, Passcode: %s}",
                        entry->alias, entry->mac, entry->passcode);
        }
    } else {
        sys_snode_t *snode;
        SYS_SLIST_FOR_EACH_NODE(&user_config_list, snode) {
            user_config_t *entry = CONTAINER_OF(snode, user_config_t, node);
            if (strcmp(entry->alias, alias) == 0) {
                shell_print(shell, "{Alias: %s, MAC Address: %s, Passcode: %s}",
                            entry->alias, entry->mac, entry->passcode);
                k_mutex_unlock(&user_config_list_mutex);
                return;
            }
        }
    }

    k_mutex_unlock(&user_config_list_mutex);
}

// Initialize list, mutex, and semaphore
void user_init(void) {
    k_mutex_init(&user_config_list_mutex);
    k_sem_init(&user_list_update_sem, 0, 1);
    sys_slist_init(&user_config_list);

    // for (int i = 0; i < sizeof(user_configs) / sizeof(user_configs[0]); i++) {
    //     user_add(user_configs[i].alias, user_configs[i].mac, user_configs[i].passcode);
    // }
}