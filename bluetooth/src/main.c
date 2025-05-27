#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>

#define STACKSIZE               4096
#define DEVICE_NAME		    CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN		(sizeof(DEVICE_NAME) - 1)

struct bt_conn *conn_connected = NULL;

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

	printk("%s() - Len: %d, Message: %.*s\n", __func__, len, len, (char *)data);
}

static void connected(struct bt_conn *conn, uint8_t err) {
    if (err) {
        printk("Connection failed (err %u)\n", err);
    } else {
        printk("Connected\n");
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

int set_up_ble(void) {
    int err;

	printk("Sample - Bluetooth Peripheral NUS\n");

	err = bt_nus_cb_register(&nus_listener, NULL);
	if (err) {
		printk("Failed to register NUS callback: %d\n", err);
		return err;
	}

	err = bt_enable(NULL);
	if (err) {
		printk("Failed to enable bluetooth: %d\n", err);
		return err;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Failed to start advertising: %d\n", err);
		return err;
	}

	printk("Initialization complete\n");

    return 0;
}

void bluetooth_write(const char *msg, size_t len) {
    int err;

    if (!conn_connected) {
        // printk("No connection available to send\n");
        return;
    }

    err = bt_nus_send(conn_connected, (const uint8_t *)msg, len);
    if (err) {
        printk("Failed to send NUS data (err %d)\n", err);
    }
}

int main(void) {
	int err = set_up_ble();
    bt_conn_cb_register(&conn_callbacks);
    k_work_init_delayable(&adv_restart_work, adv_restart_fn);

	while (1) {
		
		bluetooth_write("HIIII", 5);
		k_sleep(K_MSEC(100)); 
	}
}


// void ble_thread(void *, void *, void *) {

//     int err = set_up_ble();
//     bt_conn_cb_register(&conn_callbacks);
//     k_work_init_delayable(&adv_restart_work, adv_restart_fn);

//     while (1) {

//         k_sleep(K_MSEC(10)); 
//     }
// }

// // Define and start threads
// K_THREAD_DEFINE(ble_tid, STACKSIZE, ble_thread, NULL, NULL, NULL, 1, 0, 0);