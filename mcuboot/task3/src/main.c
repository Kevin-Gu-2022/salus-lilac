#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>   

#define PRIORITY                1     // Priority level for threads
#define STACKSIZE               1024  // Stack size for threads
#define MSGQ_SIZE               10    // Message queue size
#define ALIGNMENT_SIZE_IN_BYTES 4     // Alignment for message queue

// Define LED device tree aliases
#define LED1_NODE DT_ALIAS(led1)      // Alias for LED1 GPIO
#define LED2_NODE DT_ALIAS(led0)      // Alias for LED2 GPIO
 
// Retrieve LED GPIO configurations from the device tree
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);

// Structure to store time command parameters
struct time_cmds_t {
    const struct shell *sh;
    bool args_selection[3];
};

// Structure to store LED command parameters
struct led_cmds_t {
    const struct shell *sh;
    bool set;
    bool leds_on[2];
    bool invalid_cmd;
};

// Register logging module for debugging
LOG_MODULE_REGISTER(led_module, LOG_LEVEL_DBG);

// Define message queues for time and LED commands
K_MSGQ_DEFINE(time_cmds_msgq, sizeof(struct time_cmds_t), MSGQ_SIZE, ALIGNMENT_SIZE_IN_BYTES);
K_MSGQ_DEFINE(led_cmds_msgq, sizeof(struct led_cmds_t), MSGQ_SIZE, ALIGNMENT_SIZE_IN_BYTES);

/*
 * Initialize GPIO pins for LED control.
 */
void leds_init(void) {

    int ret;

    if (!gpio_is_ready_dt(&led1)) {
		return;
	}

    if (!gpio_is_ready_dt(&led2)) {
		return;
	}

    // Configure LEDs as output
	ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return;
	}

    ret = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return;
	}

    // Ensure LEDs are initially off
    ret = gpio_pin_set_dt(&led1, 0);
    ret = gpio_pin_set_dt(&led2, 0);

    LOG_DBG("led init OK");
}

/*
 * Handles the `time` shell command.
 */
static int cmd_time_handler(const struct shell *sh, size_t argc, char **argv)
{
    struct time_cmds_t sendData;
    memset(sendData.args_selection, 0, sizeof(sendData.args_selection));

    sendData.sh = sh;

    if (argc == 1) {

        sendData.args_selection[0] = true;     // Default: seconds format
    } else if (argc == 2 && !strcmp(argv[1], "f")) {

        sendData.args_selection[1] = true;     // Formatted time (hh:mm:ss)
    } else {

        sendData.args_selection[2] = true;     // Invalid command
    }

    // Add the command to the message queue
    if (k_msgq_put(&time_cmds_msgq, &sendData, K_FOREVER) != 0) {
        // Handle message queue failure
    }

    return 0;
}

/*
 * Thread to process time commands from the message queue.
 */
void process_time_thread(void *, void *, void *) {
    
    uint64_t seconds_total;

    struct time_cmds_t receivedData;

    while (1) {
        
        if (k_msgq_get(&time_cmds_msgq, &receivedData, K_FOREVER) != 0) {
            // Handle error if needed
        }

        // Convert uptime to seconds
        seconds_total = k_uptime_get() / 1000U;

        if (receivedData.args_selection[0]) {
            
            shell_print(receivedData.sh, "System Uptime: %llu secs", seconds_total);
        }

        if (receivedData.args_selection[1]) {

            // Formatted output 
            uint64_t hours   = seconds_total / 3600U;
            uint64_t minutes = (seconds_total / 60U) % 60U;
            uint64_t seconds = seconds_total % 60U;

            shell_print(receivedData.sh, "System Uptime: %02llu:%02llu:%02llu", hours, minutes, seconds);
        }

        if (receivedData.args_selection[2]) {

            LOG_ERR("invalid command");
        }
    }
}

// Register the `time` command in Zephyr's shell
SHELL_CMD_ARG_REGISTER(time, NULL,
    "Display system uptime.\n"
    "Usage: time [f]\n"
    "No args = seconds, 'f' = hh:mm:ss format",
    cmd_time_handler,
    1, 1);

/*
 * Handles the `led` shell command.
 */
static int cmd_leds_handler(const struct shell *sh, size_t argc, char **argv) {

    struct led_cmds_t sendData;
    memset(sendData.leds_on, 0, sizeof(sendData.leds_on));
    sendData.invalid_cmd = false;

    if (argc == 3) {

        if (!strcmp(argv[1], "s")) {

            sendData.set = true;    // Set LEDs
        } else if (!strcmp(argv[1], "t")) {

            sendData.set = false;   // Toggle LEDs
        } else {

            sendData.invalid_cmd = true;
        }

        // Parse LED states
        if (argv[2][0] == '0') {

            sendData.leds_on[0] = false;
        } else if (argv[2][0] == '1') {

            sendData.leds_on[0] = true;
        } else {

            sendData.invalid_cmd = true;
        }

        if (argv[2][1] == '0') {

            sendData.leds_on[1] = false;
        } else if (argv[2][1] == '1') {

            sendData.leds_on[1] = true;
        } else {

            sendData.invalid_cmd = true;
        }

        if (strlen(argv[2]) != 2) {

            sendData.invalid_cmd = true;
        }

        if (k_msgq_put(&led_cmds_msgq, &sendData, K_FOREVER) != 0) {
            // Handle message queue failure
        }
    } 
    return 0;
}

/*
 * Toggles the state of an LED.
 */
void toggle_leds(const struct gpio_dt_spec led, bool led_state) {

    if (led_state) {
        gpio_pin_toggle_dt(&led);
    } 
}

/*
 * Log messages for setting LEDs.
 */
void log_set_messages(bool led_state, bool led_on, int led_num) {

    if (led_on) {
        if (led_state) {
            LOG_WRN("led %u already on", led_num);
        } else {
            LOG_INF("led %u is on", led_num);
        }
    } else {
        
        if (!led_state) {
            LOG_WRN("led %u already off", led_num);
        } else {
            LOG_INF("led %u is off", led_num);
        }
    }
}

/*
 * Log messages for toggling LEDs.
 */
void log_toggle_messages(bool led_state, bool led_toggle, int led_num) {

    if (led_state) {
        if (led_toggle) {
            LOG_INF("led %u is off", led_num);
        } else {
            LOG_WRN("led %u already on", led_num);
        }
    } else {
        if (led_toggle) {
            LOG_INF("led %u is on", led_num);
        } else {
            LOG_WRN("led %u already off", led_num);
        }
    }
}

/*
 * Processes LED commands from the message queue.
 */
void process_leds_thread(void *, void *, void *) {

    leds_init();
    int ret;
    struct led_cmds_t receivedData;

    while (1) {

        if (k_msgq_get(&led_cmds_msgq, &receivedData, K_FOREVER) != 0) {
            // Handle error if needed
        }

        if (!receivedData.invalid_cmd) {
            int led1_state = gpio_pin_get_dt(&led1);
            int led2_state = gpio_pin_get_dt(&led2);    

            if (receivedData.set) {
                log_set_messages(led1_state, receivedData.leds_on[1], 1);
                log_set_messages(led2_state, receivedData.leds_on[0], 2);
                ret = gpio_pin_set_dt(&led1, receivedData.leds_on[1]);
                ret = gpio_pin_set_dt(&led2, receivedData.leds_on[0]);
            } else {
                toggle_leds(led1, receivedData.leds_on[1]);
                toggle_leds(led2, receivedData.leds_on[0]);
                log_toggle_messages(led1_state, receivedData.leds_on[1], 1);
                log_toggle_messages(led2_state, receivedData.leds_on[0], 2);
            }
        } else {
            LOG_ERR("invalid command");
        }
    } 
}

// Register the `led` command in Zephyr's shell
SHELL_CMD_ARG_REGISTER(led, NULL,
    "LED: Invalid Command.",
    cmd_leds_handler,
    3, 0);

int main(void) {
    // Main does nothing since threads are defined outside
}

// Define and start threads
K_THREAD_DEFINE(time_cmd_tid, STACKSIZE, process_time_thread, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(leds_cmd_tid, STACKSIZE, process_leds_thread, NULL, NULL, NULL, PRIORITY, 0, 0);