/*
* @file     event.c
* @brief    Event Detection and Classification
* @author   Lachlan Chun, 47484874
*/

#include "event.h"

typedef enum {
    STATE_IDLE,
    STATE_CONNECTING,
    STATE_TAMPERING,
	STATE_PRESENCE,
	STATE_PASSCODE,
	STATE_FAIL,
	STATE_SUCCESS,
	STATE_BLOCKCHAIN,
    STATE_MAX
} system_state_t;

// State names for logging/debugging
const char *state_names[] = {
    "IDLE", "CONNECTING", "TAMPERING", "PRESENCE", "PASSCODE", "FAIL", "SUCCESS", "BLOCKCHAIN"
};

// Global pointer to the currently authenticated user
user_config_t *current_user = NULL;

static system_state_t current_state = STATE_IDLE;
system_state_t previous_state = STATE_IDLE;

// Prototypes for state handlers
void transition_to(system_state_t next_state);
void handle_idle(void);
void handle_connecting(void);
void handle_connected(void);
void handle_tampering(void);
void handle_presence(void);
void handle_passcode(void);
void handle_fail(void);
void handle_success(void);
void handle_blockchain(void);

// Finite state machine thread.
void fsm_thread(void) {
	while (1) {
        switch (current_state) {
            case STATE_IDLE:
                handle_idle();
                break;
            case STATE_CONNECTING:
                handle_connecting();
                break;
            case STATE_TAMPERING:
                handle_tampering();
                break;
            case STATE_PRESENCE:
                handle_presence();
                break;
            case STATE_PASSCODE:
                handle_passcode();
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
                printk("Invalid state: %d\n", current_state);
                current_state = STATE_FAIL;
                break;
        }

		// printk("Current state: %s\n", state_names[current_state]);
		k_msleep(1000);
	}
}

// Function for transitioning to the next state
void transition_to(system_state_t next_state) {
    previous_state = current_state;
    current_state = next_state;
}

// IDLE: Wait until ultrasonic sensor signal is received
void handle_idle(void) {
    printk("State: IDLE\n");

    // Wait on a message from the ultrasonic topic
	if (1) {
		transition_to(STATE_CONNECTING);
	}
}

// CONNECTING: Attempt to connect to authorised users
void handle_connecting(void) {
    printk("State: CONNECTING\n");

    static bool started = false;
    if (!started) {
        bluetooth_init();
        bluetooth_advertise();
        started = true;
    }
    
    char mac_data[MAC_ADDRESS_LENGTH];

	// Transition on successful Bluetooth connection
	if (k_msgq_get(&user_mac_msgq, mac_data, K_NO_WAIT) == 0) {
		// Find the user in the list
        user_config_t *user;
        SYS_SLIST_FOR_EACH_CONTAINER(&user_config_list, user, node) {
            if (strcmp(user->mac, mac_data) == 0) {
                current_user = user;
                break;
            }
        }
        
        printk("Connected to authorised user with MAC: %s\n", mac_data);
        transition_to(STATE_PASSCODE);
        started = false;
        return;
	}
    
    // If message received from magnetometer topic, tampering detected
    if (0) {
		transition_to(STATE_TAMPERING);
        return;
	} 
    
    // After some time has elapsed and no successful connection, presence detected
    if (0) {
        transition_to(STATE_PRESENCE);
        return;
	}
}

// TAMPERING: Magnetometer signal received with no authorised connections
void handle_tampering(void) {
    printk("State: TAMPERING\n");
    
	// Signal to speaker and camera over MQTT
	// TODO

    transition_to(STATE_BLOCKCHAIN);
}

// PRESENCE: No authorised connections and no tampering detections
void handle_presence(void) {
    printk("State: PRESENCE\n");

    // Signal to speaker and camera over MQTT
	// TODO

    transition_to(STATE_BLOCKCHAIN);
}

// PASSCODE: Receive passcode attempts from authorised users
void handle_passcode(void) {
    printk("State: PASSCODE\n");

	// Track the number of passcode attempts
	static int passcode_attempts = 0;

    char passcode_data[PASSCODE_LENGTH];

	// Transition on successful Bluetooth connection
	if (current_user && k_msgq_get(&user_passcode_msgq, passcode_data, K_NO_WAIT) == 0) {
		printk("Received passcode: %s\n", passcode_data);
        
        if (strncmp(passcode_data, current_user->passcode, PASSCODE_LENGTH - 1) == 0) {
            transition_to(STATE_SUCCESS);
        } else {
            passcode_attempts++;

            // Check if the number of password attempts are lower than the limit
            if (passcode_attempts >= PASSCODE_ATTEMPTS) {
                transition_to(STATE_FAIL);
            } else {
                transition_to(STATE_PASSCODE);
            }
	    }
	} 
}

// FAIL: Incorrect passcode and attempt limit was reached
void handle_fail(void) {
    printk("State: FAIL\n");

    // Signal to speaker and camera over MQTT
	// TODO

    transition_to(STATE_BLOCKCHAIN);
}

// FAIL: Correct passcode within attempt limit
void handle_success(void) {
    printk("State: SUCCESS\n");
    
	// Signal to servo motor, speaker and camera over MQTT
	// TODO

    transition_to(STATE_BLOCKCHAIN);
}

// FAIL: Appends the event to the blockchain
void handle_blockchain(void) {
    printk("State: BLOCKCHAIN\n");
    
	// Store the event on the blockchain
	switch (previous_state) {
        case STATE_TAMPERING:
            // TODO
            break;
        case STATE_PRESENCE:
            // TODO
            break;
		case STATE_FAIL:
            // TODO
            break;
		case STATE_SUCCESS:
            // TODO
            break;
        default:
            // TODO
            break;
    }

    transition_to(STATE_IDLE);
}

// Definte and start the threads.
K_THREAD_DEFINE(fsm_thread_id, STACK_SIZE, fsm_thread, NULL, NULL, NULL, THREAD_PRIORITY, 0, 0);