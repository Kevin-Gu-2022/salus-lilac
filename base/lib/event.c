/*
* @file     event.c
* @brief    Event Detection and Classification
* @author   Lachlan Chun, 47484874
*/

#include "event.h"

LOG_MODULE_REGISTER(base, LOG_LEVEL_DBG);

// State names for logging/debugging
const char *state_names[] = {
    "IDLE",
    "SENSOR_CONNECT",
    "SENSOR_SYNC",
    "SENSOR_DATA",
    "SENSOR_DISCONNECT",
    "MOBILE_CONNECT",
    "MOBILE_DATA",
    "MOBILE_DISCONNECT",
    "TAMPERING",
    "PRESENCE",
    "FAIL",
    "SUCCESS",
    "BLOCKCHAIN"
};

// Global pointer to the currently authenticated user
user_config_t *current_user = NULL;
user_config_t *last_failed_user = NULL;

static system_state_t current_state = STATE_IDLE;
system_state_t previous_state = STATE_IDLE;
static event_type_t current_event = EVENT_NONE;
static int64_t current_event_time = 0;
static char last_mag_meas[SENSOR_MEAS_LENGTH] = "N/A";
static char last_ultra_meas[SENSOR_MEAS_LENGTH] = "N/A";

// Prototypes for state handlers
void transition_to(system_state_t next_state);
void handle_idle(void);
void handle_sensor_connect(void);
void handle_sensor_sync(void);
void handle_sensor_data(void);
void handle_sensor_disconnect(void);
void handle_mobile_connect(void);
void handle_mobile_data(void);
void handle_mobile_disconnect(void);
void handle_tampering(void);
void handle_presence(void);
void handle_fail(void);
void handle_success(void);
void handle_blockchain(void);

// Define message queues.
K_MSGQ_DEFINE(mobile_mac_msgq, MAC_ADDRESS_LENGTH, MSGQ_SIZE, 4);
K_MSGQ_DEFINE(sensor_msgq, MAX_NOTIFY_LEN, MSGQ_SIZE, 4);
K_MSGQ_DEFINE(mobile_msgq, MAX_NOTIFY_LEN, MSGQ_SIZE, 4);
K_SEM_DEFINE(sensor_connect_sem, 0, 1);
K_SEM_DEFINE(sensor_disconnect_sem, 0, 1);
K_SEM_DEFINE(sensor_reconnect_sem, 0, 1);
K_SEM_DEFINE(mobile_connect_sem, 0, 1);
K_SEM_DEFINE(mobile_disconnect_sem, 0, 1);
K_SEM_DEFINE(mobile_reconnect_sem, 0, 1);

static const bt_addr_le_t sensor_mac = {
    .type = BT_ADDR_LE_RANDOM,
    .a = { .val = { 0x02, 0xEF, 0xBE, 0x00, 0xAD, 0xDE } }
};

static struct bt_gatt_subscribe_params subscribe_params;

struct bt_conn *conn_connected;
void (*start_scan_func)(void);

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
            if (user == last_failed_user) {
                // If this user was the last failed attempt, ignore them
                return false;
            }
            
            // Copy the valid MAC string to the caller's buffer
            strncpy(out_mac, addr_str, out_mac_len - 1);
            out_mac[out_mac_len - 1] = '\0';  // Ensure null termination
            return true;
        }
    }

    return false;
}

static uint8_t notify_func(struct bt_conn *conn, struct bt_gatt_subscribe_params *params, const void *data, uint16_t length) {
	if (!data) {
		// LOG_ERR("[UNSUBSCRIBED]");
		params->value_handle = 0U;
		return BT_GATT_ITER_STOP;
	}

	// Ensure safe copy within buffer limits
	uint16_t copy_len = length < (MAX_NOTIFY_LEN - 1) ? length : (MAX_NOTIFY_LEN - 1);
	char last_notify_string[MAX_NOTIFY_LEN];
    memcpy(last_notify_string, data, copy_len);
	last_notify_string[copy_len] = '\0';

    int ret;

    if (current_state == STATE_SENSOR_DATA) {
        if (bt_addr_le_cmp(bt_conn_get_dst(conn), &sensor_mac) == 0) {
            ret = k_msgq_put(&sensor_msgq, last_notify_string, K_NO_WAIT);
            if (ret != 0) {
                LOG_ERR("Failed to enqueue sensor notification (err %d)", ret);
            }
        }
    } else if (current_state == STATE_MOBILE_DATA) {
        if (bt_addr_le_cmp(bt_conn_get_dst(conn), &sensor_mac) != 0) {
            ret = k_msgq_put(&mobile_msgq, last_notify_string, K_NO_WAIT);
            if (ret != 0) {
                LOG_ERR("Failed to enqueue mobile notification (err %d)", ret);
            }
        }
    }

	return BT_GATT_ITER_CONTINUE;
}

// Device filter and connection
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad) {
    char dev[BT_ADDR_LE_STR_LEN];
    struct bt_conn *conn = NULL;
    int err;

    bt_addr_le_to_str(addr, dev, sizeof(dev));

    // Only interested in connectable ads
    if (type != BT_GAP_ADV_TYPE_ADV_IND && type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        return; 
    }
    
    if (current_state == STATE_SENSOR_CONNECT) {
        if (bt_addr_le_cmp(addr, &sensor_mac) == 0) {
            err = bt_le_scan_stop();
            if (err) {
                LOG_ERR("Scan stop failed (err %d)", err);
                return;
            }

            err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &conn);
            if (err) {
                LOG_ERR("Create connection failed (err %d)", err);
                start_scan_func();
            } else {
                bt_conn_unref(conn);  // Reference passed to callbacks
            }
        }
    } else if (current_state == STATE_MOBILE_CONNECT) {
        if (is_mac_allowed(addr, dev, sizeof(dev))) {            
            err = bt_le_scan_stop();
            if (err) {
                LOG_ERR("Scan stop failed (err %d)", err);
                return;
            }

            err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &conn);
            if (err) {
                LOG_ERR("Create connection failed (err %d)", err);
                start_scan_func();
            } else {
                bt_conn_unref(conn);  // Reference passed to callbacks
                k_msgq_put(&mobile_mac_msgq, dev, K_NO_WAIT);
            }
        }
    }
}

static void start_scan(void) {
    int err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
    if (err) {
        LOG_ERR("Scan start failed (err %d)", err);
    } else {
        // LOG_INF("Scanning...");
    }
}

// Callbacks
static void connected(struct bt_conn *conn, uint8_t err) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err) {
        LOG_ERR("Connect to %s failed (err %u)", addr, err);
        start_scan_func();
        return;
    }

    LOG_INF("Connected device: %s", addr);
    conn_connected = bt_conn_ref(conn);

	if (conn == conn_connected) {
		subscribe_params.notify = notify_func;
		subscribe_params.value_handle = 0x0012;      // TX value handle
		subscribe_params.ccc_handle = 0x0013;        // CCC descriptor
		subscribe_params.value = BT_GATT_CCC_NOTIFY;

		int err = bt_gatt_subscribe(conn, &subscribe_params);
		if (err && err != -EALREADY) {
			LOG_ERR("Subscribe failed (err %d)", err);
		} else {
			// LOG_INF("[SUBSCRIBED to 0x0012]");

            if (bt_addr_le_cmp(bt_conn_get_dst(conn), &sensor_mac) == 0) {
                // Sensor connected, signal semaphore
                k_sem_give(&sensor_connect_sem);
            } else {
                // Mobile connected, signal semaphore
                k_sem_give(&mobile_connect_sem);
            }
		}
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("%s disconnected.", addr);

    
    if (conn_connected) {
        bt_conn_unref(conn_connected);
        conn_connected = NULL;
    }

    if (current_state == STATE_SENSOR_DISCONNECT) {
        k_sem_give(&sensor_disconnect_sem);
    } else if (current_state == STATE_SENSOR_DATA) {
        k_sem_give(&sensor_reconnect_sem);
    } else if (current_state == STATE_MOBILE_DISCONNECT) {
        k_sem_give(&mobile_disconnect_sem);
    } else if (current_state == STATE_MOBILE_DATA) {
        k_sem_give(&mobile_reconnect_sem);
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static int write_cmd(struct bt_conn *conn, const char *string) {
    if (!string || !conn) {
        return -EINVAL;
    }

    return bt_gatt_write_without_response(conn, TARGET_HANDLE, string, strlen(string), false);
}

static int write_int(struct bt_conn *conn, uint64_t *time) {
    if (!time || !conn) {
        return -EINVAL;
    }

    return bt_gatt_write_without_response(conn, TARGET_HANDLE, time, sizeof(uint64_t), false);
}

void config_mac_addr(void) {
	int err;

	// Set up a static address for the Bluetooth device
    bt_addr_le_t device_address = {
        .type = BT_ADDR_LE_RANDOM,
        .a = { .val = { 0x01, 0xEF, 0xBE, 0x00, 0xAD, 0xDE } }
    };

    err = bt_id_create(&device_address, NULL);
    if (err < 0) {
        LOG_ERR("Failed to create custom identity (err %d)", err);
    }
}

void bluetooth_scan(void) {
    start_scan_func = start_scan;
    start_scan();
}

void bluetooth_write(const char *string) {
	if (conn_connected) {
		struct bt_conn *conn = bt_conn_ref(conn_connected);
		if (conn) {
			int err = write_cmd(conn, string);
			bt_conn_unref(conn);
			if (err) {
				LOG_ERR("Write failed (err %d)", err);
			} else {
				// LOG_INF("Write successful");
			}
		}
	}
}

void bluetooth_write_int(uint64_t *data) {
	if (conn_connected) {
		struct bt_conn *conn = bt_conn_ref(conn_connected);
		if (conn) {
			int err = write_int(conn, data);
			bt_conn_unref(conn);
			if (err) {
				LOG_ERR("Write failed (err %d)", err);
			} else {
				// LOG_INF("Write successful");
			}
		}
	}
}

void bluetooth_disconnect(void) {
    if (conn_connected) {
        int err = bt_conn_disconnect(conn_connected, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        if (err) {
            LOG_ERR("Disconnection failed (err %d)", err);
        } else {
            // LOG_INF("Disconnection initiated.");
        }
    }
}

void bluetooth_init(void) {
	config_mac_addr();
    int err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth enable failed (err %d)", err);
        return;
    }
    LOG_INF("Bluetooth initialised");	
}

int magnetometer_event(const char *data_str) {
    double x, y, z;

    // Parse the comma-separated string
    if (sscanf(data_str, "%lf,%lf,%lf", &x, &y, &z) != 3) {
        return -EINVAL; // Parsing error
    }

    // Compute squared magnitude
    double mag_sq = x * x + y * y + z * z;

    // Compare with baseline
    double diff_sq = fabs(mag_sq - MAG_BASELINE_SQ);

    snprintf(last_mag_meas, sizeof(last_mag_meas), "%.3f", diff_sq);

    if (diff_sq > sensor_get_magnetometer_threshold()) {
        // Significant change detected
        return 1;
    }

    return 0; // No significant change
}

int ultrasonic_event(const char *data_str) {
    double distance;

    // Parse the comma-separated string
    if (sscanf(data_str, "%lf", &distance) != 1) {
        return -EINVAL; // Parsing error
    }

    snprintf(last_ultra_meas, sizeof(last_ultra_meas), "%.3f", distance);

    if (distance <= sensor_get_ultrasonic_threshold()) {
        // Significant change detected
        return 1;
    }

    return 0; // No significant change
}

// Finite state machine thread.
void fsm_thread(void) {
	while (1) {
        switch (current_state) {
            case STATE_IDLE:
                handle_idle();
                break;
            case STATE_SENSOR_CONNECT:
                handle_sensor_connect();
                break;
            case STATE_SENSOR_SYNC:
                handle_sensor_sync();
                break;
            case STATE_SENSOR_DATA:
                handle_sensor_data();
                break;
            case STATE_SENSOR_DISCONNECT:
                handle_sensor_disconnect();
                break;
            case STATE_MOBILE_CONNECT:
                handle_mobile_connect();
                break;
            case STATE_MOBILE_DATA:
                handle_mobile_data();
                break;
            case STATE_MOBILE_DISCONNECT:
                handle_mobile_disconnect();
                break;
            case STATE_TAMPERING:
                handle_tampering();
                break;
            case STATE_PRESENCE:
                handle_presence();
                break;
            case STATE_FAIL:
                handle_fail();
                break;
            case STATE_SUCCESS:
                handle_success();
                break;
            case STATE_BLOCKCHAIN:
                handle_blockchain();
                break;
            default:
                LOG_ERR("Invalid state: %d", current_state);
                current_state = STATE_FAIL;
                break;
        }
		k_msleep(100);
	}
}

// Function for transitioning to the next state
void transition_to(system_state_t next_state) {
    previous_state = current_state;
    current_state = next_state;
}

// IDLE: Starting state
void handle_idle(void) {
    LOG_INF("Resetting system.");
    k_msleep(2500);
    current_event = EVENT_NONE;
    strcpy(last_ultra_meas, "N/A");
    strcpy(last_mag_meas, "N/A");
    transition_to(STATE_SENSOR_CONNECT);
}

// SENSOR_CONNECT: Scan and connect to the sensor board
void handle_sensor_connect(void) {
    LOG_INF("Trying to connect to sensors...");

    bluetooth_scan();

    k_sem_take(&sensor_connect_sem, K_FOREVER);

    transition_to(STATE_SENSOR_SYNC);
    LOG_INF("Sensors found! Sychronising time...");
}

// SENSOR_SYNC: Synchronise time between sensor and base nodes
void handle_sensor_sync(void) {
    static int sync_attempts = 0;    

    // Send the current time across
    uint64_t currentTime = k_uptime_get();
    bluetooth_write_int(&currentTime);
    sync_attempts++;

    if (sync_attempts >= SENSOR_SYNC_ATTEMPTS) {
        LOG_INF("Sensor time sychronised!");
        transition_to(STATE_SENSOR_DATA);
    }
}

// SENSOR_DATA: Receive/process sensor data
void handle_sensor_data(void) {
    char notify_buffer[MAX_NOTIFY_LEN];

    if (k_msgq_get(&sensor_msgq, notify_buffer, K_NO_WAIT) == 0) {
        
        int comma_count = 0;
        for (char *p = notify_buffer; *p != '\0'; ++p) {
            if (*p == ',') {
                comma_count++;
            }
        }
        if (comma_count == 2) {
            // Magnetometer data
            if (magnetometer_event(notify_buffer)) {
                LOG_INF("Tampering detected!");
                current_event = EVENT_TAMPERING;
                current_event_time = k_uptime_get() / 1000;
                transition_to(STATE_SENSOR_DISCONNECT);
                return;
            }
        } else if (comma_count == 0) {
            // Ultrasonic data
            if (ultrasonic_event(notify_buffer)) {
                LOG_INF("Presence detected!");
                current_event = EVENT_PRESENCE;
                current_event_time = k_uptime_get() / 1000;
                transition_to(STATE_SENSOR_DISCONNECT);
                return;
            }
        }
    }

    // Sensor disconnected, try and reconnect
    if (k_sem_take(&sensor_reconnect_sem, K_NO_WAIT) == 0) {
        LOG_INF("Sensors disconnected, attempting reconnect...");
        transition_to(STATE_SENSOR_CONNECT);
    }
}

// SENSOR_DISCONNECT: Disconnect and prepare for mobile connect
void handle_sensor_disconnect(void) {
    bluetooth_disconnect();

    k_sem_take(&sensor_disconnect_sem, K_FOREVER);

    k_msleep(1000);

    transition_to(STATE_MOBILE_CONNECT);
}

// MOBILE_CONNECT: Scan and connect to mobile devices (allowed MACs)
void handle_mobile_connect(void) {
    LOG_INF("Searching for authorised users...");

    bluetooth_scan();

    if (k_sem_take(&mobile_connect_sem, MOBILE_CONNECT_TIMEOUT) == 0) {
        char mac_data[MAC_ADDRESS_LENGTH];

        // Transition on successful Bluetooth connection
        if (k_msgq_get(&mobile_mac_msgq, mac_data, K_NO_WAIT) == 0) {
            // Find the user in the list
            user_config_t *user;

            k_mutex_lock(&user_config_list_mutex, K_FOREVER);
            SYS_SLIST_FOR_EACH_CONTAINER(&user_config_list, user, node) {
                if (strcmp(user->mac, mac_data) == 0) {
                    current_user = user;
                    break;
                }
            }
            k_mutex_unlock(&user_config_list_mutex);
        }

        LOG_INF("%s is connected! Please enter your passcode.", current_user->alias);

        // On mobile device connect event:
        transition_to(STATE_MOBILE_DATA);
    } else {
        if (current_event == EVENT_TAMPERING) {
            // If tampering detected, go to TAMPERING state
            transition_to(STATE_TAMPERING);
        } else if (current_event == EVENT_PRESENCE) {
            // If presence detected, go to PRESENCE state
            transition_to(STATE_PRESENCE);
        }
    }    
}

// MOBILE_DATA: Communicate with mobile device
void handle_mobile_data(void) {
    // Track the number of passcode attempts
	static int passcode_attempts = 0;
    static char input_buffer[PASSCODE_LENGTH] = {0};
    static int input_index = 0;

    char key = keypad_scan();

    if (key != '\0' && ((key >= '0' && key <= '9') || key == 'D' || key == 'E' || key == 'C')) {
        if (key == 'C') {  // 'C' for Clear
            input_index = 0;
            memset(input_buffer, 0, sizeof(input_buffer));
        } else if (key == 'D') {  // 'D' for Delete
            if (input_index > 0) {
                input_index--;
                input_buffer[input_index] = '\0';
            }
        } else if (key >= '0' && key <= '9') {
            if (input_index < PASSCODE_LENGTH - 1) {
                input_buffer[input_index++] = key;
                input_buffer[input_index] = '\0';
            } else {
                // Ignore if passcode is already full
                return;
            }
        } else if (key == 'E') { // 'E' for Enter
            if (input_index != PASSCODE_LENGTH - 1) {
                // Ignore enter if passcode not fully entered
                return;
            }

            if (strncmp(input_buffer, current_user->passcode, PASSCODE_LENGTH - 1) == 0) {
                LOG_INF("Passcode correct! Welcome %s!", current_user->alias);
                bluetooth_write("-:----");
                current_event = EVENT_SUCCESS;
                current_event_time = k_uptime_get() / 1000;
                passcode_attempts = 0;
                // Clear input after processing
                input_index = 0;
                memset(input_buffer, 0, sizeof(input_buffer));
                k_msleep(100);
                bluetooth_write("Y");
                transition_to(STATE_MOBILE_DISCONNECT);
                return;
            } else {
                passcode_attempts++;
                if (passcode_attempts >= PASSCODE_ATTEMPTS) {
                    LOG_INF("%s has been temporarily locked out. Please seek assistance.", current_user->alias);
                    bluetooth_write("-:----");
                    current_event = EVENT_FAIL;
                    current_event_time = k_uptime_get() / 1000;
                    passcode_attempts = 0;
                    // Clear input after processing
                    input_index = 0;
                    memset(input_buffer, 0, sizeof(input_buffer));
                    k_msleep(100);
                    bluetooth_write("F");
                    transition_to(STATE_MOBILE_DISCONNECT);
                    return;
                } else {
                    LOG_INF("Passcode incorrect! Please try again %s.", current_user->alias);
                    bluetooth_write("N");
                }
            }

            // Clear input after processing
            input_index = 0;
            memset(input_buffer, 0, sizeof(input_buffer));
        }

        char display_buffer[DISPLAY_BUFFER_SIZE];
        int attempts_remaining = PASSCODE_ATTEMPTS - passcode_attempts;
        display_buffer[0] = '0' + attempts_remaining;
        display_buffer[1] = ':';

        for (int i = 0; i < PASSCODE_LENGTH - 1; ++i) {
            if (i < input_index) {
                display_buffer[i + 2] = input_buffer[i];
            } else {
                display_buffer[i + 2] = '-';
            }
        }
        display_buffer[DISPLAY_BUFFER_SIZE - 1] = '\0';
        bluetooth_write(display_buffer);
    }

    // Mobile disconnected, try and reconnect
    if (k_sem_take(&mobile_reconnect_sem, K_NO_WAIT) == 0) {
        transition_to(STATE_MOBILE_CONNECT);
    }
}

// MOBILE_DISCONNECT: Disconnect mobile and go idle
void handle_mobile_disconnect(void) {
    k_msleep(5000);

    bluetooth_disconnect();

    k_sem_take(&mobile_disconnect_sem, K_FOREVER);

    k_msleep(1000);

    switch (current_event) {
        case EVENT_FAIL:
            transition_to(STATE_FAIL);
            break;
        case EVENT_SUCCESS:
            transition_to(STATE_SUCCESS);
            break;
        default:
            LOG_ERR("Unkown state.");
            break;
    }
}

// TAMPERING: Magnetometer signal received with no authorised connections
void handle_tampering(void) {
    transition_to(STATE_BLOCKCHAIN);
}

// PRESENCE: No authorised connections and no tampering detections
void handle_presence(void) {
    transition_to(STATE_BLOCKCHAIN);
}

// FAIL: Incorrect passcode and attempt limit was reached
void handle_fail(void) {
    last_failed_user = current_user;
    transition_to(STATE_BLOCKCHAIN);
}

// FAIL: Correct passcode within attempt limit
void handle_success(void) {
    LOG_INF("Unlocking door!");
    servo_toggle();
    last_failed_user = NULL;
    k_msleep(2000);
    transition_to(STATE_BLOCKCHAIN);
}

// FAIL: Appends the event to the blockchain
void handle_blockchain(void) {
    char event_time[16];
    snprintf(event_time, sizeof(event_time), "%lld", current_event_time);
    
	// Store the event on the blockchain
	switch (previous_state) {
        case STATE_TAMPERING:
            add_block(event_time, "TAMPERING", last_mag_meas, last_ultra_meas, "Intruder", "N/A");
            LOG_INF("Tampering event added to blockchain.");
            k_msleep(5000);
            break;
        case STATE_PRESENCE:
            add_block(event_time, "PRESENCE", last_mag_meas, last_ultra_meas, "Visitor", "N/A");
            LOG_INF("Presence event added to blockchain.");
            k_msleep(5000);
            break;
		case STATE_FAIL:
            add_block(event_time, "FAIL", last_mag_meas, last_ultra_meas, current_user->alias, current_user->mac);
            LOG_INF("Fail event added to blockchain.");
            k_msleep(5000);
            break;
		case STATE_SUCCESS:
            add_block(event_time, "SUCCESS", last_mag_meas, last_ultra_meas, current_user->alias, current_user->mac);
            LOG_INF("Success event added to blockchain.");
            k_msleep(5000);
            LOG_INF("Locking door!");
            servo_toggle();
            break;
        default:
            LOG_ERR("BLOCKCHAIN: Unknown event.");
            break;
    }

    current_user = NULL;
    transition_to(STATE_IDLE);
}

// Definte and start the threads.
K_THREAD_DEFINE(fsm_thread_id, STACK_SIZE, fsm_thread, NULL, NULL, NULL, THREAD_PRIORITY, 0, 0);