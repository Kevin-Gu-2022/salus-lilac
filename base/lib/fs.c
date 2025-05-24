/*
* @file     fs.c
* @brief    File System Library
* @author   Lachlan Chun, 47484874
*/

#include "fs.h"

#define CONFIG_FILE_PATH "/lfs/users"

static const struct json_obj_descr json_descr[] = {
    JSON_OBJ_DESCR_PRIM_NAMED(user_config_t, "Alias", alias, JSON_TOK_STRING),
    JSON_OBJ_DESCR_PRIM_NAMED(user_config_t, "MAC Address", mac, JSON_TOK_STRING),
    JSON_OBJ_DESCR_PRIM_NAMED(user_config_t, "Passcode", passcode, JSON_TOK_STRING)
};

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
static struct fs_mount_t lfs_storage_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &storage,
	.storage_dev = (void *)FIXED_PARTITION_ID(storage_partition),
	.mnt_point = "/lfs",
};

ssize_t fs_read_line(struct fs_file_t *file, char *buf, size_t max_len) {
    size_t total = 0;
    char c;
    while (total < max_len - 1) {
        ssize_t r = fs_read(file, &c, 1);
        if (r <= 0) break;
        if (c == '\n') break;
        buf[total++] = c;
    }
    buf[total] = '\0';
    return total > 0 ? total : -1;
}

// Initialise users when powered on.
int fs_user_init(void) {
    struct fs_file_t file;
    fs_file_t_init(&file);

    int err = fs_open(&file, CONFIG_FILE_PATH, FS_O_READ);
    if (err < 0) {
        if (err == -ENOENT) {
            // File doesn't exist, create it
            printk("User config not found, creating new file...\n");
            err = fs_open(&file, CONFIG_FILE_PATH, FS_O_CREATE | FS_O_WRITE);
            if (err < 0) {
                printk("Failed to create user config file: %d\n", err);
                return err;
            }
            fs_close(&file);
            return 0;
        } else {
            printk("Error opening user config file: %d\n", err);
            return err;
        }
    }

    char line_buf[256];
    ssize_t read_len;
    user_config_t new_user;

    while ((read_len = fs_read_line(&file, line_buf, sizeof(line_buf))) > 0) {
        memset(&new_user, 0, sizeof(new_user));

        int ret = json_obj_parse(line_buf, strlen(line_buf), json_descr, ARRAY_SIZE(json_descr), &new_user);
        if (ret < 0) {
            printk("Failed to parse JSON line: %d\n", ret);
            continue;
        }

        // Add to your list using user_add
        if (user_add(new_user.alias, new_user.mac, new_user.passcode) != 0) {
            printk("Failed to add user to list: %s\n", new_user.alias);
        }
    }

    fs_close(&file);
    return 0;
}

// Initialises LittleFS.
void fs_init(void) {
    int rc;
    rc = fs_mount(&lfs_storage_mnt);

    fs_user_init();
}

// File System thread.
void fs_thread(void) {

    while (1) {
        k_sem_take(&user_list_update_sem, K_FOREVER);

        struct fs_file_t file;
        fs_file_t_init(&file);

        int err = fs_open(&file, CONFIG_FILE_PATH, FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
        if (err < 0) {
            printk("Failed to open file for writing: %d\n", err);
            continue;
        }

        sys_snode_t *node;
        SYS_SLIST_FOR_EACH_NODE(&user_config_list, node) {
            user_config_t *entry = CONTAINER_OF(node, user_config_t, node);
            
            char json_buf[256];
            int len = json_obj_encode_buf(json_descr, ARRAY_SIZE(json_descr), entry, json_buf, sizeof(json_buf));
            if (len < 0) {
                printk("Failed to encode JSON: %d\n", len);
                continue;
            }

            fs_write(&file, json_buf, strlen(json_buf));
            fs_write(&file, "\n", 1);
        }

        fs_close(&file);
    }

}

// Definte and start the thread.
K_THREAD_DEFINE(fs_thread_id, STACK_SIZE, fs_thread, NULL, NULL, NULL, THREAD_PRIORITY, 0, 0);