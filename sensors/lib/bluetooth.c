/*
* @file     bluetooth.c
* @brief    Bluetooth Library
* @author   Xinlin Zhong, 48018061
*/

#include "bluetooth.h"
#include "time_sync.h"



LOG_MODULE_REGISTER(bluetooth, LOG_LEVEL_DBG);

struct bt_conn *conn_connected = NULL;

static const bt_addr_t target_mac = {
    .val = { 0x01, 0xEF, 0xBE, 0x00, 0xAD, 0xDE }
};

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

<<<<<<< Updated upstream
    uint32_t base_unit_time_received = *(uint32_t*)data;
    uint32_t sensor_local_time_at_reception = (uint32_t) k_uptime_get(); // Capture local time immediately!

    // Add this pair to our regression data.
    // X is the Base Unit's time (received), Y is our Sensor Node's local time (at reception).
    time_sync_add_data_point(&sensor_node_time_sync_state, base_unit_time_received, sensor_local_time_at_reception);

    // Add this pair to our regression data.
    // X is the Base Unit's time (received), Y is our Sensor Node's local time (at reception).
    time_sync_add_data_point(&sensor_node_time_sync_state, base_unit_time_received, sensor_local_time_at_reception);

	printk("%s() - Len: %d, Message: %.*s\n", __func__, len, len, (char *)data);
=======
	printk("%s() - Len: %d, Message: %.*lld\n", __func__, len, len, *(uint64_t*)data);
>>>>>>> Stashed changes
}

static void connected(struct bt_conn *conn, uint8_t err) {
    if (err) {
        printk("Connection failed (err %u)\n", err);
    }

    const bt_addr_le_t *peer_addr = bt_conn_get_dst(conn);
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(peer_addr, addr_str, sizeof(addr_str));


    if (bt_addr_cmp(&peer_addr->a, &target_mac) == 0) {
        printk("Authorised MAC!");
        conn_connected = bt_conn_ref(conn);
    } else {
        printk("Unauthorised MAC: %s - Disconnecting\n", addr_str);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
}

void adv_restart_fn(struct k_work *work) {
    int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        printk("Deferred advertising restart failed (err %d)\n", err);
    } else {
        printk("Advertising restarted (deferred)\n");
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    printk("Disconnected (reason %u)\n", reason);
    if (conn_connected) {
        bt_conn_unref(conn_connected);
        conn_connected = NULL;
    }
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

int bluetooth_advertise(void) {
    
    int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Failed to start advertising: %d\n", err);
		return err;
	}
    return 0;
    
}

void bluetooth_write(const char *msg, size_t len) {
    int err;

    if (!conn_connected) {
        printk("No connection available to send\n");
        return;
    }

    err = bt_nus_send(conn_connected, (const uint8_t *)msg, len);
    if (err) {
        printk("Failed to send NUS data (err %d)\n", err);
    }
}

void config_mac_addr(void) {
	int err;

	// set up a static address for the Bluetooth device
    bt_addr_le_t device_address = {
        .type = BT_ADDR_LE_RANDOM,
        .a = { .val = { 0x02, 0xEF, 0xBE, 0x00, 0xAD, 0xDE } }
    };

    err = bt_id_create(&device_address, NULL);
    if (err < 0) {
        printk("Failed to create custom identity (err %d)\n", err);
    }
}

int bluetooth_init(void) {   
    int err;

    // Initialise the time syncing stuff
    time_sync_init(&sensor_node_time_sync_state);

	printk("Sample - Bluetooth Peripheral NUS\n");

	err = bt_nus_cb_register(&nus_listener, NULL);
	if (err) {
		printk("Failed to register NUS callback: %d\n", err);
		return err;
	}

    config_mac_addr();
	err = bt_enable(NULL);
	if (err) {
		printk("Failed to enable bluetooth: %d\n", err);
		return err;
	}

    // Initialise delayed work, register callbacks
    bt_conn_cb_register(&conn_callbacks);
    k_work_init_delayable(&adv_restart_work, adv_restart_fn);

    printk("Bluetooth initialised\n");
    bluetooth_advertise();

    return 0;
}




// #define MIN_RSSI      -70
// #define TARGET_HANDLE 0x0015

// #define MAX_NOTIFY_LEN 256
// static char last_notify_string[MAX_NOTIFY_LEN];
// static struct bt_gatt_subscribe_params subscribe_params;

// static uint8_t notify_func(struct bt_conn *conn,
// 			   struct bt_gatt_subscribe_params *params,
// 			   const void *data, uint16_t length) {
// 	if (!data) {
// 		printk("[UNSUBSCRIBED]\n");
// 		params->value_handle = 0U;
// 		return BT_GATT_ITER_STOP;
// 	}

// 	// Ensure safe copy within buffer limits
// 	uint16_t copy_len = length < (MAX_NOTIFY_LEN - 1) ? length : (MAX_NOTIFY_LEN - 1);
// 	memcpy(last_notify_string, data, copy_len);
// 	last_notify_string[copy_len] = '\0';  // Null-terminate

// 	printk("[NOTIFICATION STORED] %s\n", last_notify_string);

// 	// printk("%s() - Len: %d, Message (NOT PASSCODE): %.*s\n", __func__, len, len, (char *)data);

// 	return BT_GATT_ITER_CONTINUE;
// }

// static const bt_addr_t target_mac = {
//     .val = { 0x01, 0xEF, 0xBE, 0x00, 0xAD, 0xDE }
// };

// struct bt_conn *conn_connected;
// void (*start_scan_func)(void);

// // Device filter and connection ---
// static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
//                          struct net_buf_simple *ad) {
//     char dev[BT_ADDR_LE_STR_LEN];
//     struct bt_conn *conn = NULL;
//     int err;

//     bt_addr_le_to_str(addr, dev, sizeof(dev));
//     printk("[SCAN] %s, RSSI %d\n", dev, rssi);

    // if ((type != BT_GAP_ADV_TYPE_ADV_IND && type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) ||
    //     (rssi < MIN_RSSI) ||
    //     (bt_addr_cmp(&addr->a, &target_mac) != 0) ||
    //     (addr->type != BT_ADDR_LE_RANDOM)) {
    //     return;
    // }

//     printk("[MATCH] Target device found. Connecting...\n");

//     err = bt_le_scan_stop();
//     if (err) {
//         printk("Scan stop failed (err %d)\n", err);
//         return;
//     }

//     err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &conn);
//     if (err) {
//         printk("Create connection failed (err %d)\n", err);
//         start_scan_func();
//     } else {
//         bt_conn_unref(conn);  // Reference passed to callbacks
//     }
// }

// static void start_scan(void) {
//     int err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
//     if (err) {
//         printk("Scan start failed (err %d)\n", err);
//     } else {
//         printk("Scanning...\n");
//     }
// }

// // callbacks
// static void connected(struct bt_conn *conn, uint8_t err) {
//     char addr[BT_ADDR_LE_STR_LEN];
//     bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

//     if (err) {
//         printk("[FAIL] Connect to %s (err %u)\n", addr, err);
//         start_scan_func();
//         return;
//     }

//     printk("[OK] Connected: %s\n", addr);
//     conn_connected = bt_conn_ref(conn);

// 	if (conn == conn_connected) {
// 		subscribe_params.notify = notify_func;
// 		subscribe_params.value_handle = 0x0012;      // TX value handle
// 		subscribe_params.ccc_handle = 0x0013;        // CCC descriptor
// 		subscribe_params.value = BT_GATT_CCC_NOTIFY;

// 		int err = bt_gatt_subscribe(conn, &subscribe_params);
// 		if (err && err != -EALREADY) {
// 			printk("Subscribe failed (err %d)\n", err);
// 		} else {
// 			printk("[SUBSCRIBED to 0x0012]\n");
// 		}
// 	}
// }

// static void disconnected(struct bt_conn *conn, uint8_t reason) {
//     char addr[BT_ADDR_LE_STR_LEN];
//     bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
//     printk("[DISC] %s (reason %u)\n", addr, reason);

//     if (conn_connected) {
//         bt_conn_unref(conn_connected);
//         conn_connected = NULL;
//     }

//     start_scan_func();
// }

// BT_CONN_CB_DEFINE(conn_callbacks) = {
//     .connected = connected,
//     .disconnected = disconnected,
// };

// static int write_cmd(struct bt_conn *conn, const char *string) {
//     if (!string || !conn) {
//         return -EINVAL;
//     }

//     return bt_gatt_write_without_response(conn, TARGET_HANDLE,
//                                           string, strlen(string), false);
// }

// void bluetooth_scan(void) {
//     start_scan_func = start_scan;
//     start_scan();
// }

// void bluetooth_init(void) {
//     int err = bt_enable(NULL);
//     if (err) {
//         printk("Bluetooth enable failed (err %d)\n", err);
//         return;
//     }
//     printk("Bluetooth initialized\n");
    
//     bluetooth_scan();
// }


// void bluetooth_write(const char *string) {

// 	if (conn_connected) {
// 		struct bt_conn *conn = bt_conn_ref(conn_connected);
// 		if (conn) {
// 			int err = write_cmd(conn, string);
// 			bt_conn_unref(conn);
// 			if (err) {
// 				printk("Write failed (err %d)\n", err);
// 			} else {
// 				printk("Write successful\n");
// 			}
// 		}
// 	}
// }

