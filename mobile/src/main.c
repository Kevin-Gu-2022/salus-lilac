/*
* @file     main.c
* @brief    Project - Mobile
* @author   Xinlin Zhong, 48018061
*/

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>
#include <stdio.h>
#include <string.h>
#include <lvgl.h>
#include "lock_img.c"
#include "bluetooth_img.c"
#include "attempts_img.c"
#include "warning_img.c"
#include "enter_img.c"
#include "pass_img.c"
#include "dashline_img.c"

LOG_MODULE_REGISTER(mobile);
void show_incorrect_passcode_img(void);

#define MAX_CIRCLES 4
#define MSGQ_SIZE               20    // Message queue size
#define MAX_NAME_LEN 16
#define STACKSIZE               4096
#define DEVICE_NAME		    CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN		(sizeof(DEVICE_NAME) - 1)

static lv_obj_t *lock_imgage;
static lv_obj_t *ble_imgage;
static lv_obj_t *enter_text;
static lv_obj_t *attempts_text;
static lv_obj_t *dashline;
static lv_obj_t *warning;
static lv_obj_t *pass;
static lv_obj_t *num_attempts;
static lv_obj_t *first_digit;
static lv_obj_t *second_digit;
static lv_obj_t *third_digit;
static lv_obj_t *fourth_digit;

K_MSGQ_DEFINE(data_msgq, MAX_NAME_LEN, MSGQ_SIZE, 4);
K_SEM_DEFINE(conn_status_sem, 0, 1);
bool is_connected = false;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
};

static struct k_work_delayable adv_restart_work;

static void notif_enabled(bool enabled, void *ctx) {
	ARG_UNUSED(ctx);

	printk("%s() - %s\n", __func__, (enabled ? "Enabled" : "Disabled"));
}

static void received(struct bt_conn *conn, const void *data, uint16_t len, void *ctx) {
	ARG_UNUSED(conn);
	ARG_UNUSED(ctx);

	// printk("%s() - Len: %d, Message: %.*s\n", __func__, len, len, (char *)data);

    if (len > 0 && len < MAX_NAME_LEN) {
        char received[MAX_NAME_LEN] = {0};
        memcpy(received, data, len);
        received[len] = '\0';  
        k_msgq_put(&data_msgq, &received, K_NO_WAIT);
    }
}

static void connected(struct bt_conn *conn, uint8_t err) {
    if (err) {
        printk("Connection failed (err %u)\n", err);
    } else {
        printk("Connected\n");
        is_connected = true;
        k_sem_give(&conn_status_sem);
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
    is_connected = false;
    k_sem_give(&conn_status_sem);
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

static void hide_image_cb(lv_timer_t *timer) {
    lv_obj_t *img = (lv_obj_t *)lv_timer_get_user_data(timer);
    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);  // hide iamge
    lv_timer_del(timer);
}

void show_incorrect_passcode_img(void) {
    lv_obj_clear_flag(lock_imgage, LV_OBJ_FLAG_HIDDEN);  // show image
    lv_timer_t *timer = lv_timer_create(hide_image_cb, 1500, lock_imgage);
    lv_timer_set_repeat_count(timer, 1);
}

void init_interface_thread(void *, void *, void *) {

    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display not ready");
        return;
    }

    // init_images();
    display_blanking_off(display_dev);

    while (1) {
        lv_timer_handler();
        k_sleep(K_MSEC(10));
    }
}

void config_images(void) {
    ble_imgage = lv_img_create(lv_scr_act());
    lv_img_set_src(ble_imgage, &bluetooth_img);  
    lv_obj_set_pos(ble_imgage, 270, 10);

    attempts_text = lv_img_create(lv_scr_act());
    lv_img_set_src(attempts_text, &attempts_img);  
    lv_obj_set_pos(attempts_text, 5, 4);

    enter_text = lv_img_create(lv_scr_act());
    lv_img_set_src(enter_text, &enter_img);  
    lv_obj_set_pos(enter_text, 77, 80);

    dashline = lv_img_create(lv_scr_act());
    lv_img_set_src(dashline, &dashline_img);  
    lv_obj_set_pos(dashline, 40, 190);

    warning = lv_img_create(lv_scr_act());
    lv_img_set_src(warning, &warning_img);  
    lv_obj_set_pos(warning, 65, 25);

    pass = lv_img_create(lv_scr_act());
    lv_img_set_src(pass, &pass_img);  
    lv_obj_set_pos(pass, 5, 10);

    lock_imgage = lv_img_create(lv_scr_act());
    lv_img_set_src(lock_imgage, &lock_img);  
    lv_obj_set_pos(lock_imgage, 230, 10);
}

void config_labels(void) {
    num_attempts = lv_label_create(lv_scr_act());
    first_digit = lv_label_create(lv_scr_act());
    second_digit = lv_label_create(lv_scr_act());
    third_digit = lv_label_create(lv_scr_act());
    fourth_digit = lv_label_create(lv_scr_act());

    lv_obj_set_pos(num_attempts, 190, 25);
    lv_obj_set_pos(first_digit, 55, 160);
    lv_obj_set_pos(second_digit, 120, 160);
    lv_obj_set_pos(third_digit, 180, 160);
    lv_obj_set_pos(fourth_digit, 240, 160);

    lv_obj_set_style_text_font(num_attempts, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_font(first_digit, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_font(second_digit, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_font(third_digit, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_font(fourth_digit, &lv_font_montserrat_20, LV_PART_MAIN);
}

void disable_labels(void) {
    lv_label_set_text(num_attempts, "");
    lv_label_set_text(first_digit, "");
    lv_label_set_text(second_digit, "");
    lv_label_set_text(third_digit, "");
    lv_label_set_text(fourth_digit, "");
}

void only_display_fail(void) {
    lv_obj_clear_flag(warning, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ble_imgage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pass, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(attempts_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(enter_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(dashline, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lock_imgage, LV_OBJ_FLAG_HIDDEN);
}

void only_display_pass(void) {
    lv_obj_clear_flag(pass, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ble_imgage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(attempts_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(enter_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(dashline, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lock_imgage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(warning, LV_OBJ_FLAG_HIDDEN);
}

void update_interface_thread(void *, void *, void *) {

    config_images();
    config_labels();

    is_connected = false;
    k_sem_give(&conn_status_sem);

    char received[MAX_NAME_LEN] = {0};

    while (1) {
        // Clear buffer
        memset(received, 0, sizeof(received));  

        if (!k_msgq_get(&data_msgq, &received, K_NO_WAIT)) {
            printk("Recevied Data: %s\n", received);
            if (strlen(received) == 6 && received[1] == ':') {
                lv_label_set_text(num_attempts, (received[0] == '-') ? "" : (char[2]){received[0], '\0'});
                lv_label_set_text(first_digit, (received[2] == '-') ? "" : (char[2]){received[2], '\0'});
                lv_label_set_text(second_digit, (received[3] == '-') ? "" : (char[2]){received[3], '\0'});
                lv_label_set_text(third_digit, (received[4] == '-') ? "" : (char[2]){received[4], '\0'});
                lv_label_set_text(fourth_digit, (received[5] == '-') ? "" : (char[2]){received[5], '\0'});
            } else if (strlen(received) == 1) {
                switch (received[0]) {
                    case 'N':
                        show_incorrect_passcode_img();
                        break;
                    case 'Y':
                        only_display_pass();
                        break;
                    case 'F':
                        only_display_fail();
                        break;
                }
            }
        }
        k_sleep(K_MSEC(10)); 
    }
}

void ui_setup_connection_thread(void *, void *, void *) {

    while (1) {
        k_sem_take(&conn_status_sem, K_FOREVER);
        if (is_connected) {
            lv_obj_clear_flag(ble_imgage, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(attempts_text, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(enter_text, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(dashline, LV_OBJ_FLAG_HIDDEN);
        } else {
            // Hide everything when disconnected
            lv_obj_add_flag(ble_imgage, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(attempts_text, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(enter_text, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(dashline, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(warning, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(pass, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lock_imgage, LV_OBJ_FLAG_HIDDEN);
        }
        disable_labels();
        k_sleep(K_MSEC(100)); 
    }
}

void ble_thread(void *, void *, void *) {
    set_up_ble();
    bt_conn_cb_register(&conn_callbacks);
    k_work_init_delayable(&adv_restart_work, adv_restart_fn);

    while (1) {
        k_sleep(K_MSEC(100)); 
    }
}

K_THREAD_DEFINE(ble_tid, 2048, ble_thread, NULL, NULL, NULL, 5, 0, 0); // priority 2 (lower)
K_THREAD_DEFINE(init_tid, 4096, init_interface_thread, NULL, NULL, NULL, 7, 0, 0);
K_THREAD_DEFINE(update_tid, 2048, update_interface_thread, NULL, NULL, NULL, 7, 0, 0);
K_THREAD_DEFINE(ui_setup_tid, 1024, ui_setup_connection_thread, NULL, NULL, NULL, 7, 0, 0);

