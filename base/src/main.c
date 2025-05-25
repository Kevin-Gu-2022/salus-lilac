/*
* @file     main.c
* @brief    Smart Entry System Project - Base Node
* @author   Lachlan Chun, 47484874
*/

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/shell/shell.h>
#include "user.h"
#include "fs.h"
#include "bluetooth.h"
#include "servo.h"

// Adding users command.
static int cmd_user_add(const struct shell *shell, size_t argc, char **argv) {
    const char *alias = NULL;
    const char *mac = NULL;
    const char *passcode = NULL;

    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "-a") == 0) {
            alias = argv[i + 1];
        } else if (strcmp(argv[i], "-m") == 0) {
            mac = argv[i + 1];
        } else if (strcmp(argv[i], "-p") == 0) {
            passcode = argv[i + 1];
        }
    }

    if (!alias || !mac || !passcode) {
        shell_print(shell, "Usage: user add -a <alias> -m <MAC> -p <passcode>");
        return -EINVAL;
    }

    int ret = user_add(alias, mac, passcode);
    switch (ret) {
        case USER_SUCCESS:
            shell_print(shell, "Entry access for user '%s' has been granted.", alias);
            break;
        case USER_ALREADY_EXISTS:
            shell_print(shell, "User '%s' already has entry access permissions.", alias);
            break;
        case USER_MAC_ALREADY_EXISTS:
            shell_print(shell, "MAC Address '%s' is already in use.", mac);
            break;
        case USER_MAC_INVALID:
            shell_print(shell, "MAC Address '%s' is not valid.", mac);
            break;
        case USER_PASSCODE_INVALID:
            shell_print(shell, "Passcode is not valid.");
            break;
        case USER_MEMORY_ERROR:
            shell_print(shell, "Memory allocation failed.");
            break;
        default:
            shell_print(shell, "Unknown error.");
            break;
    }

    return 0;
}

// Removing users command.
static int cmd_user_remove(const struct shell *shell, size_t argc, char **argv) {
    if (argc != 2) {
        shell_print(shell, "Usage: user remove <alias>");
        return -EINVAL;
    }

    int ret = user_remove(argv[1]);
    if (ret == USER_SUCCESS) {
        shell_print(shell, "Entry access for user '%s' has been revoked.", argv[1]);
    } else {
        shell_print(shell, "User '%s' not found in the list.", argv[1]);
    }

    return 0;
}

// Viewing users command.
static int cmd_user_view(const struct shell *shell, size_t argc, char **argv) {
    if (argc != 2) {
        shell_print(shell, "Usage: user view <alias | -a>");
        return -EINVAL;
    }

    user_view(shell, argv[1]);
    return 0;
}

// Main.
int main(void) {
    user_init();
    fs_init();
    servo_init();
    bluetooth_init();
}

// Subcommands for user configuration.
SHELL_STATIC_SUBCMD_SET_CREATE(
    user_cmds,
    SHELL_CMD(add, NULL, "Add a user: -a <alias> -m <MAC address> -p <passcode>", cmd_user_add),
    SHELL_CMD(remove, NULL, "Remove user: user remove <alias>", cmd_user_remove),
    SHELL_CMD(view, NULL, "View user/s: user view <alias> or user view -a", cmd_user_view),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(user, &user_cmds, "User entry access configuration commands.", NULL);