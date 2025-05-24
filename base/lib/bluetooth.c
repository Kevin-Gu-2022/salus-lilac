/*
* @file     bluetooth.c
* @brief    Bluetooth Library
* @author   Xinlin Zhong, 48018061
*/

#include "bluetooth.h"

// Define message queues.
K_MSGQ_DEFINE(user_mac_msgq, MAC_ADDRESS_LENGTH, MSGQ_SIZE, 4);
K_MSGQ_DEFINE(user_passcode_msgq, PASSCODE_LENGTH, MSGQ_SIZE, 4);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
};

static struct k_work_delayable adv_restart_work;

static void notif_enabled(bool enabled, void *ctx)
{
	ARG_UNUSED(ctx);

	printk("%s() - %s\n", __func__, (enabled ? "Enabled" : "Disabled"));
}

static void received(struct bt_conn *conn, const void *data, uint16_t len, void *ctx)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(ctx);

    if (data != NULL && len == 4) {
        char passcode[PASSCODE_LENGTH];
        memcpy(passcode, data, 4);
        passcode[4] = '\0'; 
        k_msgq_put(&user_passcode_msgq, passcode, K_NO_WAIT);
        return;
    }

	printk("%s() - Len: %d, Message (NOT PASSCODE): %.*s\n", __func__, len, len, (char *)data);
}

bool is_mac_allowed(const bt_addr_le_t *addr, char *out_mac, size_t out_mac_len) {
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

    // Strip off the " (random)" or " (public)" suffix
    char *paren = strchr(addr_str, ' ');
    if (paren) {
        *paren = '\0';
    }

    user_config_t *user;
    SYS_SLIST_FOR_EACH_CONTAINER(&user_config_list, user, node) {
        if (strcmp(user->mac, addr_str) == 0) {
            // Copy the valid MAC string to the caller's buffer
            strncpy(out_mac, addr_str, out_mac_len - 1);
            out_mac[out_mac_len - 1] = '\0';  // Ensure null termination
            return true;
        }
    }

    return false;
}

static void connected(struct bt_conn *conn, uint8_t err) {
    if (err) {
        printk("Connection failed (err %u)\n", err);
    }

    const bt_addr_le_t *peer_addr = bt_conn_get_dst(conn);
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(peer_addr, addr_str, sizeof(addr_str));
    
    char mac[MAC_ADDRESS_LENGTH];
    if (is_mac_allowed(peer_addr, mac, sizeof(mac))) {
        k_msgq_put(&user_mac_msgq, mac, K_NO_WAIT);
    } else {
        printk("Unauthorised MAC: %s - Disconnecting\n", addr_str);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
}

void adv_restart_fn(struct k_work *work) {
    int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        printk("Deferred advertising restart failed (err %d)\n", err);
    } else {
        printk("Advertising restarted (deferred)\n");
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    printk("Disconnected (reason %u)\n", reason);
    k_work_schedule(&adv_restart_work, K_MSEC(500));
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

struct bt_nus_cb nus_listener = {
	.notif_enabled = notif_enabled,
	.received = received,
};

int bluetooth_init(void) {   
    int err;

	printk("Sample - Bluetooth Peripheral NUS\n");

	err = bt_nus_cb_register(&nus_listener, NULL);
	if (err) {
		printk("Failed to register NUS callback: %d\n", err);
		return err;
	}

    // Set up a static address for the Bluetooth device
    bt_addr_le_t device_address = {
        .type = BT_ADDR_LE_RANDOM,
        .a = { .val = { 0x01, 0xEF, 0xBE, 0x00, 0xAD, 0xDE } }
    };

    err = bt_id_create(&device_address, NULL);
    if (err < 0) {
        printk("Failed to create custom identity (err %d)\n", err);
    }

	err = bt_enable(NULL);
	if (err) {
		printk("Failed to enable bluetooth: %d\n", err);
		return err;
	}

    // Initialise delayed work, register callbacks
    bt_conn_cb_register(&conn_callbacks);
    k_work_init_delayable(&adv_restart_work, adv_restart_fn);

    printk("Bluetooth initialized (but not advertising)\n");

    return 0;
}

int bluetooth_advertise(void) {
    
    int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Failed to start advertising: %d\n", err);
		return err;
	}
    return 0;
    
}