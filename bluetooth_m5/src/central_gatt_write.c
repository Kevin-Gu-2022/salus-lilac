#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/hci.h>

#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>

#define MIN_RSSI      -70
#define TARGET_HANDLE 0x0015
#define MAX_NOTIFY_LEN 256
static char last_notify_string[MAX_NOTIFY_LEN];
static struct bt_gatt_subscribe_params subscribe_params;

K_SEM_DEFINE(conn_sem, 0, 1); 

static uint8_t notify_func(struct bt_conn *conn,
			   struct bt_gatt_subscribe_params *params,
			   const void *data, uint16_t length) {
	if (!data) {
		printk("[UNSUBSCRIBED]\n");
		params->value_handle = 0U;
		return BT_GATT_ITER_STOP;
	}

	// Ensure safe copy within buffer limits
	uint16_t copy_len = length < (MAX_NOTIFY_LEN - 1) ? length : (MAX_NOTIFY_LEN - 1);
	memcpy(last_notify_string, data, copy_len);
	last_notify_string[copy_len] = '\0';  // Null-terminate

	printk("[NOTIFICATION STORED] %s\n", last_notify_string);

	return BT_GATT_ITER_CONTINUE;
}

static const bt_addr_t target_mac = {
    .val = { 0x01, 0xEF, 0xBE, 0x00, 0xAD, 0xDE }
};

struct bt_conn *conn_connected;
void (*start_scan_func)(void);

// Device filter and connection ---
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                         struct net_buf_simple *ad) {
    char dev[BT_ADDR_LE_STR_LEN];
    struct bt_conn *conn = NULL;
    int err;

    bt_addr_le_to_str(addr, dev, sizeof(dev));
    printk("[SCAN] %s, RSSI %d\n", dev, rssi);

    if ((type != BT_GAP_ADV_TYPE_ADV_IND && type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) ||
        (rssi < MIN_RSSI) ||
        (bt_addr_cmp(&addr->a, &target_mac) != 0) ||
        (addr->type != BT_ADDR_LE_RANDOM)) {
        return;
    }

    printk("[MATCH] Target device found. Connecting...\n");

    err = bt_le_scan_stop();
    if (err) {
        printk("Scan stop failed (err %d)\n", err);
        return;
    }

    err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &conn);
    if (err) {
        printk("Create conn failed (err %d)\n", err);
        start_scan_func();
    } else {
        bt_conn_unref(conn);  // Reference passed to callbacks
    }
}

static void start_scan(void) {
    int err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
    if (err) {
        printk("Scan start failed (err %d)\n", err);
    } else {
        printk("Scanning...\n");
    }
}

// callbacks
static void connected(struct bt_conn *conn, uint8_t err) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err) {
        printk("[FAIL] Connect to %s (err %u)\n", addr, err);
        start_scan_func();
        return;
    }

    printk("[OK] Connected: %s\n", addr);
    conn_connected = bt_conn_ref(conn);
	k_sem_give(&conn_sem); 

	if (conn == conn_connected) {
		subscribe_params.notify = notify_func;
		subscribe_params.value_handle = 0x0012;      // TX value handle
		subscribe_params.ccc_handle = 0x0013;        // CCC descriptor
		subscribe_params.value = BT_GATT_CCC_NOTIFY;

		int err = bt_gatt_subscribe(conn, &subscribe_params);
		if (err && err != -EALREADY) {
			printk("Subscribe failed (err %d)\n", err);
		} else {
			printk("[SUBSCRIBED to 0x0012]\n");
		}
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printk("[DISC] %s (reason %u)\n", addr, reason);

    if (conn_connected) {
        bt_conn_unref(conn_connected);
        conn_connected = NULL;
    }

    start_scan_func();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static int write_cmd(struct bt_conn *conn, const char *string) {
    if (!string || !conn) {
        return -EINVAL;
    }

    return bt_gatt_write_without_response(conn, TARGET_HANDLE,
                                          string, strlen(string), false);
}

void config_mac_addr(void) {
	int err;

	// set up a static address for the Bluetooth device
    bt_addr_le_t device_address = {
        .type = BT_ADDR_LE_RANDOM,
        .a = { .val = { 0x01, 0xEF, 0xBE, 0x00, 0xFE, 0xCA } }
    };

    err = bt_id_create(&device_address, NULL);
    if (err < 0) {
        printk("Failed to create custom identity (err %d)\n", err);
    }
}

void init_ble_with_scanning(void) {
	// config_mac_addr();
    int err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth enable failed (err %d)\n", err);
        return;
    }
    printk("Bluetooth initialized\n");
	
	start_scan_func = start_scan;
    start_scan();
}

void gatt_write_cmd(const char *string) {

	k_sem_take(&conn_sem, K_FOREVER);  

	if (conn_connected) {
		struct bt_conn *conn = bt_conn_ref(conn_connected);
		if (conn) {
			int err = write_cmd(conn, string);
			bt_conn_unref(conn);
			if (err) {
				printk("Write failed (err %d)\n", err);
			} else {
				printk("Write successful\n");
			}
		}
	}
}

