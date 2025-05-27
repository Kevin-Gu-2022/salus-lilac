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
#include <lvgl_input_device.h>
#include "unlock_img.c"
#include "lock_img.c"
#include "bluetooth_img.c"


LOG_MODULE_REGISTER(mobile);

#define MAX_CIRCLES 4

#define STACKSIZE               4096
#define DEVICE_NAME		    CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN		(sizeof(DEVICE_NAME) - 1)

static uint32_t count = 0;
static lv_obj_t *pin_circles[MAX_CIRCLES];
static uint8_t pin_index = 0;
static char pin_input[5] = {0}; 
static lv_obj_t *lock_imgage;

struct bt_conn *conn_connected = NULL;
static struct k_work_delayable adv_restart_work;

struct k_mutex lvgl_mutex;

void show_incorrect_passcode_img(void);

K_SEM_DEFINE(ble_sem, 0, 1);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
};

static void notif_enabled(bool enabled, void *ctx) {
	ARG_UNUSED(ctx);
	printk("%s() - %s\n", __func__, (enabled ? "Enabled" : "Disabled"));
}

static void received(struct bt_conn *conn, const void *data, uint16_t len, void *ctx) {
	ARG_UNUSED(conn);
	ARG_UNUSED(ctx);
	printk("%s() - Len: %d, Message: %.*s\n", __func__, len, len, (char *)data);


    if (strncmp(data, "1234", 4) != 0) {
        show_incorrect_passcode_img();
        printk("%s() - Len: %d, Message: %.*s\n", __func__, len, len, (char *)data);
    }
}

static void connected(struct bt_conn *conn, uint8_t err) {
    if (err) {
        printk("Connection failed (err %u)\n", err);
    } else {
        printk("Connected\n");
        k_sem_give(&ble_sem);
        conn_connected = bt_conn_ref(conn);
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

static void bt_ready(int err) {
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }

    printk("Bluetooth ready\n");

    int adv_err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (adv_err) {
        printk("Advertising failed to start (err %d)\n", adv_err);
    }
}

// int set_up_ble(void) {
//     int err;

//     printk("Sample - Bluetooth Peripheral NUS\n");

//     bt_conn_cb_register(&conn_callbacks);

//     err = bt_nus_cb_register(&nus_listener, NULL);
//     if (err) {
//         printk("Failed to register NUS callback: %d\n", err);
//         return err;
//     }

//     // ✅ Async, non-blocking BLE init
//     err = bt_enable(bt_ready);
//     if (err) {
//         printk("Failed to start Bluetooth init: %d\n", err);
//         return err;
//     }

//     return 0;
// }


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
        printk("No connection available to send\n");
        return;
    }

    err = bt_nus_send(conn_connected, (const uint8_t *)msg, len);
    if (err) {
        printk("Failed to send NUS data (err %d)\n", err);
    }
}

static void update_circles(bool reset) {
    for (int i = 0; i < MAX_CIRCLES; i++) {
        lv_obj_set_style_bg_color(pin_circles[i], reset || i >= pin_index ? lv_color_white() : lv_color_black(), 0);
    }
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

static void lv_btn_click_callback(lv_event_t *e) {

    const char *label_text = lv_label_get_text(lv_obj_get_child(lv_event_get_target(e), 0));

    if (!strcmp(label_text, "1")) {
        show_incorrect_passcode_img();
    }

    // if (!strcmp(label_text, "C")) {
    //     count= 0;
    //     pin_index = 0;
    //     pin_input[0] = '\0'; 
    //     update_circles(true);
    // } else if ((!strcmp(label_text, "Enter"))) {
    //     if (count == 4) {
    //         LOG_INF("Passcode Entered!!!");
    //     }
    // } else if (count < 4) {
    //     count++;
    //     pin_index++;
    //     update_circles(false);

    //     strncat(pin_input, label_text, 4 - strlen(pin_input));
    // }

    LOG_INF("PIN so far: %s", pin_input);
    LOG_INF("Button '%s' pressed. Count = %u", label_text, count);
}


// Define button properties
const int btn_w = 70;
const int btn_h = 50;
const int spacing_x = 9;
const int spacing_y = 9;
const int start_x = 7;
const int start_y = 60;

// Optional: create a large text style
static lv_style_t style_big_text;

void create_first_button(void) {
    // Initialize the style
    lv_style_init(&style_big_text);
    lv_style_set_text_font(&style_big_text, &lv_font_montserrat_20);
    lv_style_set_text_align(&style_big_text, LV_TEXT_ALIGN_CENTER);

    // Create the first button ("1")
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, btn_w, btn_h);
    lv_obj_set_pos(btn, start_x, start_y);
    // Optional: add event callback if you have one
    // lv_obj_add_event_cb(btn, lv_btn_click_callback, LV_EVENT_CLICKED, NULL);

    // Create and attach the label
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "1");
    lv_obj_center(label);
    lv_obj_add_style(label, &style_big_text, 0);
}

void update_interface_thread(void *, void *, void *) {

    k_sem_take(&ble_sem, K_FOREVER);

    k_sleep(K_MSEC(500));

    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display not ready");
        return;
    }

    display_blanking_off(display_dev);

    // create_first_button();

    // // 4 columns, 3 rows
    // const int btn_w = 70;
    // const int btn_h = 50;
    // const int spacing_x = 9;
    // const int spacing_y = 9;
    // const int start_x = 7;
    // const int start_y = 60;

    // // Button labels in desired order
    // const char *btn_labels[12] = {
    //     "1", "2", "3", "C",
    //     "4", "5", "6", "0",
    //     "7", "8", "9", "Enter"
    // };

    // // Optional: create a large style
    // static lv_style_t style_big_text;

    // lv_style_init(&style_big_text);
    // lv_style_set_text_font(&style_big_text, &lv_font_montserrat_20);  // larger font
    // lv_style_set_text_align(&style_big_text, LV_TEXT_ALIGN_CENTER);

    // for (uint32_t i = 0; i < 12; i++) {
    //     lv_obj_t *btn = lv_btn_create(lv_scr_act());
    //     int col = i % 4;
    //     int row = i / 4;
    //     int x = start_x + col * (btn_w + spacing_x);
    //     int y = start_y + row * (btn_h + spacing_y);

    //     lv_obj_set_size(btn, btn_w, btn_h);
    //     lv_obj_set_pos(btn, x, y);
    //     lv_obj_add_event_cb(btn, lv_btn_click_callback, LV_EVENT_CLICKED, NULL);

    //     LOG_ERR("Buttons created!");

    //     lv_obj_t *label = lv_label_create(btn);
    //     lv_label_set_text(label, btn_labels[i]);
    //     lv_obj_center(label);  // align to middle of button
    //     lv_obj_add_style(label, &style_big_text, 0);
    // }

    // // Create 4 empty circles (unfilled initially)
    // for (int i = 0; i < MAX_CIRCLES; i++) {
    //     lv_obj_t *circle = lv_obj_create(lv_scr_act());
    //     lv_obj_set_size(circle, 20, 20);
    //     lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    //     lv_obj_set_style_border_width(circle, 2, 0);
    //     lv_obj_set_style_border_color(circle, lv_color_black(), 0);
    //     lv_obj_set_style_bg_color(circle, lv_color_white(), 0); // empty
    //     lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);
    //     lv_obj_align(circle, LV_ALIGN_TOP_MID, (i - 1.5) * 30, 23);  // space them horizontally

    //     pin_circles[i] = circle;
    // }

    lv_obj_t *ble_imgage = lv_img_create(lv_scr_act());
    lv_img_set_src(ble_imgage, &bluetooth_img);  
    lv_obj_set_pos(ble_imgage, 10, 10);

    lv_obj_t *unlock_imgage = lv_img_create(lv_scr_act());
    lv_img_set_src(unlock_imgage, &unlock_img);  
    lv_obj_set_pos(unlock_imgage, 270, 10);

    lock_imgage = lv_img_create(lv_scr_act());
    lv_img_set_src(lock_imgage, &lock_img);  
    lv_obj_set_pos(lock_imgage, 230, 10);
    lv_obj_add_flag(lock_imgage, LV_OBJ_FLAG_HIDDEN);
    
    // display_blanking_off(display_dev);

    LOG_ERR("Display initialised!");

    while (1) {
        lv_timer_handler();
        k_sleep(K_MSEC(10));
    }
}


void ble_thread(void *, void *, void *) {
    // k_sleep(K_SECONDS(15)); 
    int err = set_up_ble();
    k_work_init_delayable(&adv_restart_work, adv_restart_fn);

    while (1) {
        bluetooth_write("2020", 4);
        k_sleep(K_MSEC(10));
    }
}


// Define and start threads
// K_THREAD_DEFINE(update_tid, 4096, update_interface_thread, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(ble_tid, 2048, ble_thread, NULL, NULL, NULL, 7, 0, 0); // priority 2 (lower)

