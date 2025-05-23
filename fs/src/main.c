
#include "rtc_lib.h"  
#include "sensor_interface.h" 
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>
#include <zephyr/data/json.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include "SEGGER_RTT.h"

#define STACKSIZE               1024  // Stacksize for threads
#define MSGQ_SIZE               10    // Message queue size
#define ALIGNMENT_SIZE_IN_BYTES 4     // Alignment for message queue

struct sample_cmds_t {
    int8_t active;
    int rate;
    int8_t did;
};

typedef struct {
    bool start_logging;
    int8_t did;
    const char *file_path;
} file_log_t;

file_log_t output = {
    .start_logging = false,
    .did = -1,
    .file_path = NULL
};

static uint8_t did;  
const struct shell *shell = NULL;

static struct gpio_callback button_cb_data;
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

K_SEM_DEFINE(sample_sem, 0, 1);

// Define message queues for time and LED commands
K_MSGQ_DEFINE(sample_cmds_msgq, sizeof(struct sample_cmds_t), MSGQ_SIZE, ALIGNMENT_SIZE_IN_BYTES);

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    k_sem_give(&sample_sem);
}

void init_button(void) {
    int ret;

	if (!gpio_is_ready_dt(&button)) {
		printk("Error: button device %s is not ready\n",
		       button.port->name);
		return;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
		       ret, button.port->name, button.pin);
		return;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, button.port->name, button.pin);
		return;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
}

LOG_MODULE_REGISTER(main);

static int cmd_rtc_handler(const struct shell *sh, size_t argc, char **argv) {

    struct rtc_time current;
	if (argc == 2 && !strcmp(argv[1], "r"))  {
		int ret = rtc_read(rtc, &current);

		if (ret == -ENODATA) {
			shell_error(sh, "RTC Time Has Not Been Set Yet");
		} else if (ret != 0) {
			shell_error(sh, "Failed To Get RTC time: %d", ret);
		} else {
			shell_print(sh, "RTC Time: %04d-%02d-%02d %02d:%02d:%02d",
				current.tm_year + 1900, current.tm_mon + 1, current.tm_mday,
				current.tm_hour, current.tm_min, current.tm_sec);
		}
	}

    if (argc == 3 && !strcmp(argv[1], "w")) {
        int year, month, day, hour, min, sec;
        if (sscanf(argv[2], "%d-%d-%d|%d:%d:%d", &year, &month, &day, &hour, &min, &sec) == 6) {
            current.tm_year = year - 1900;
            current.tm_mon = month - 1;
            current.tm_mday = day;
            current.tm_hour = hour;
            current.tm_min = min;
            current.tm_sec = sec;
            rtc_set_time(rtc, &current);
        } else {
            shell_error(sh, "INVALID COMMANDS");
            return -EINVAL;
        }
    }

	return 0;
}

SHELL_CMD_ARG_REGISTER(rtc, NULL, 
	"RTC Operations:\n"
    "r  : Read RTC time.\n"
	"w  : Set RTC time. Format: YYYY-MM-DD|HH:MM:SS", 
	cmd_rtc_handler,
	2, 1
);

void handle_sensor_data(sensor_id_t sensor_id, bool sample, char *output_str, size_t size) {
    k_sem_give(sensor_info[sensor_id].sem);
    k_sem_take(sensor_info[sensor_id].ready_sem, K_FOREVER);

    const struct sensor_data_packet *packet = &received_packets[sensor_id];

    if (packet) { 
        if (!sample) {
            if (sensor_id == SENSOR_MAGNETOMETER) {
                shell_print(shell, "%s [µT] => X:%.3f Y:%.3f Z:%.3f",
                            sensor_info[sensor_id].label,
                            packet->magn[0], packet->magn[1], packet->magn[2]);
            } else {
                shell_print(shell, "%s %.3f %s",
                            sensor_info[sensor_id].label,
                            packet->value,
                            sensor_info[sensor_id].unit);
            }
        } else {
            if (sensor_id == SENSOR_MAGNETOMETER) {
                snprintf(output_str, size,
                    "%.3f %.3f %.3f [µT]",
                    packet->magn[0], packet->magn[1], packet->magn[2]);
            } else {
                snprintf(output_str, size,
                    "%.3f %s",
                    packet->value,
                    sensor_info[sensor_id].unit);
            }
        }
    }
}

static int cmd_sensor_handler(const struct shell *sh, size_t argc, char **argv) {

    shell = sh;
    int did = atoi(argv[1]);

    switch (did) {
        case SENSOR_TEMP:
        case SENSOR_HUMIDITY:
        case SENSOR_BAROMETER:
        case SENSOR_MAGNETOMETER:
            handle_sensor_data((sensor_id_t)did, 0, NULL, 0);
            break;
        case SENSOR_ALL:
            handle_sensor_data(SENSOR_TEMP, false, NULL, 0);
            handle_sensor_data(SENSOR_HUMIDITY, false, NULL, 0);
            handle_sensor_data(SENSOR_BAROMETER, false, NULL, 0);
            handle_sensor_data(SENSOR_MAGNETOMETER, false, NULL, 0);
            break;
        default:
            shell_error(shell, "INVALID SENSOR ID");
            return -EINVAL;
    }

    return 0;
}

SHELL_CMD_ARG_REGISTER(sensor, NULL, 
    "Sampling Operations:\n"
	"sensor <DID>: Read Selected sensor",
	cmd_sensor_handler,
	2, 0
);

bool is_valid_sensor_id(int did) {
    return (did == SENSOR_TEMP || 
            did == SENSOR_HUMIDITY || 
            did == SENSOR_BAROMETER || 
            did == SENSOR_MAGNETOMETER || 
            did == SENSOR_ALL);
}

bool is_integer(const char *str) {
    if (str == NULL || *str == '\0') return false;

    while (*str) {
        if (!isdigit((unsigned char)*str)) {
            return false;
        }
        str++;
    }
    return true;
}

static int cmd_sampling_handler(const struct shell *sh, size_t argc, char **argv) {

    if ((!strcmp(argv[1], "s") || !strcmp(argv[1], "p"))) {
        if (!is_valid_sensor_id(atoi(argv[2])) || !is_integer(argv[2])) {
            shell_error(sh, "Invalid Sensor ID");
            return -EINVAL;
        }
    }

    struct sample_cmds_t sendData;
    shell = sh;
    sendData.rate = -1;
    sendData.active = -1;
    sendData.did = -1;
    if (!strcmp(argv[1], "s")) {
        sendData.active = 1;
        sendData.did = atoi(argv[2]);
        did = sendData.did;
     
        if (k_msgq_put(&sample_cmds_msgq, &sendData, K_FOREVER) != 0) {
            // Handle message queue failure
        }
    } else if (!strcmp(argv[1], "p")) {
        if (did == atoi(argv[2])) {
            sendData.active = 0;
            sendData.did = atoi(argv[2]);
            if (k_msgq_put(&sample_cmds_msgq, &sendData, K_FOREVER) != 0) {
                // Handle message queue failure
            }
        } else {
            shell_error(shell, "Wrong Sensor ID");
            return -EINVAL;
        }

    } else if (!strcmp(argv[1], "w") && is_integer(argv[2])) {
        sendData.rate = atoi(argv[2]);
        if (sendData.rate <= 0) {
            shell_error(shell, "Invalid Rate");
            return -EINVAL;
        }
        if (k_msgq_put(&sample_cmds_msgq, &sendData, K_FOREVER) != 0) {
            // Handle message queue failure
        }
    } else {
        shell_error(shell, "Invalid Command");
        return -EINVAL;
    }
        
    return 0;
}

SHELL_CMD_ARG_REGISTER(sample, NULL, 
	"Sampling Operations:\n"
    "<s/p> <DID> : start (s) or stop (p) continuous sampling of sensor\n"
	"w  <rate>   : set sampling time (seconds)",
	cmd_sampling_handler,
	3, 0
);

struct sensor_json_data {
    uint8_t did;
    const char* rtc_time;
    char* device_values[4];
    size_t size;
};

static const struct json_obj_descr json_descr[] = {
    JSON_OBJ_DESCR_PRIM_NAMED(struct sensor_json_data, "DID", did, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM_NAMED(struct sensor_json_data, "RTC_TIME", rtc_time, JSON_TOK_STRING),
    JSON_OBJ_DESCR_ARRAY_NAMED(struct sensor_json_data, "DEVICE_VALUE", device_values, 4, size, JSON_TOK_STRING)
};

// Message queue for storing random numbers, 10 slots, 4 bytes alignment (8 digits)
K_MSGQ_DEFINE(log_output_msgq, 256, MSGQ_SIZE, ALIGNMENT_SIZE_IN_BYTES);

void sampling_thread(void *, void *, void *) {
    struct rtc_time current;
    struct sample_cmds_t receivedData;
    int8_t sample_active = 0;
    int8_t device_id = -1;
    int sample_rate = 1;

    while (1) {

        if (k_sem_take(&sample_sem, K_NO_WAIT) == 0) {
            if (device_id == -1) {
                shell_error(shell, "Cannot Sample Since DID Not Specified");
            } else {
                sample_active = !sample_active;
                if (!sample_active) {
                    shell_print(shell, "Sensor Sampling DID %d Stopped!!!", device_id);
                } else {
                    shell_print(shell, "Sensor Sampling DID %d Started!!!", device_id);
                }
            }
        }

        if (k_msgq_get(&sample_cmds_msgq, &receivedData, K_NO_WAIT) == 0) {
            if (receivedData.active != -1) {
                sample_active = receivedData.active;
            }
            if (receivedData.rate != -1) {
                sample_rate = receivedData.rate;
            }
            if (receivedData.did != -1) {
                device_id = receivedData.did;
            }
        }

        if (sample_active) {

            rtc_read(rtc, &current);
            char rtc_time_str[256]; 
            snprintf(rtc_time_str, sizeof(rtc_time_str),
                     "%04d-%02d-%02d|%02d:%02d:%02d",
                     current.tm_year + 1900, current.tm_mon + 1, current.tm_mday,
                     current.tm_hour, current.tm_min, current.tm_sec);

            struct sensor_json_data json_data = {
                .did = device_id,
                .rtc_time = rtc_time_str,
                .size = 1
            };

            char values[4][128]; // 4 sensor values

            sensor_id_t valid_ids[] = {SENSOR_TEMP, SENSOR_HUMIDITY, SENSOR_BAROMETER, SENSOR_MAGNETOMETER};

            if (device_id == SENSOR_ALL) {
                for (int i = 0; i < 4; i++) {
                    handle_sensor_data(valid_ids[i], true, values[i], sizeof(values[i]));
                    json_data.device_values[i] = values[i]; // assign pointer to buffer
                }
                json_data.size = 4;
            } else {
                handle_sensor_data(device_id, true, values[0], sizeof(values[0]));
                json_data.device_values[0] = values[0];
                json_data.size = 1;
            }

            char buffer[256];
            json_obj_encode_buf(json_descr, ARRAY_SIZE(json_descr),
                                          &json_data, buffer, sizeof(buffer));
            shell_print(shell, "%s", buffer);

            SEGGER_RTT_Write(0, buffer, strlen(buffer));
            SEGGER_RTT_Write(0, "\n", 1);

            if (output.start_logging && (output.did == device_id)) {
                if (k_msgq_put(&log_output_msgq, &buffer, K_FOREVER) != 0) {
                    // Handle message queue put failure
                }
            }

            k_sleep(K_SECONDS(sample_rate));

        } else {
            k_sleep(K_MSEC(100));
        } 
    }
}

void log_output_thread(void *, void *, void *) {

    char received_output[256];
    int rc;

    while (1) {

        k_msgq_get(&log_output_msgq, &received_output, K_FOREVER);

        struct fs_file_t log_file;
        fs_file_t_init(&log_file);

        rc = fs_open(&log_file, output.file_path, FS_O_WRITE | FS_O_APPEND);
        if (rc < 0) {
            printk("Error Opening File: %d\n", rc);
            continue;
        }

        rc = fs_write(&log_file, received_output, strlen(received_output));
        if (rc >= 0) {
            fs_write(&log_file, "\n", 1); 
        } else {
            shell_error(shell, "Failed To Write: %d\n", rc);
        }

        fs_close(&log_file);
    }
}

// Define and start threads
K_THREAD_DEFINE(log_output_tid, 2048, log_output_thread, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(sample_id, 3072, sampling_thread, NULL, NULL, NULL, 5, 0, 0);


FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
static struct fs_mount_t lfs_storage_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &storage,
	.storage_dev = (void *)FIXED_PARTITION_ID(storage_partition),
	.mnt_point = "/lfs",
};


static int cmd_fscmd_create(const struct shell *sh, size_t argc, char **argv) {

    const char *file_path = argv[1];
    struct fs_file_t file;
    int rc;

    fs_file_t_init(&file);
    rc = fs_open(&file, file_path, FS_O_CREATE | FS_O_RDWR);
    if (rc < 0) {
        shell_error(sh, "Failed To Create File %s: %d", file_path, rc);
        return rc;
    }

    shell_print(sh, "File Created: %s", file_path);
    fs_close(&file);

    return 0;
}


static void get_parent_path(const char *full_path, char *parent_path, size_t size) {
    strncpy(parent_path, full_path, size);
    char *last_slash = strrchr(parent_path, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';  // Terminate at last '/'
    } else {
        parent_path[0] = '\0'; // No parent
    }
}

int validate_file_path(const char *path) {
    struct fs_dir_t dir;
    char parent_path[128];
    int rc;

    get_parent_path(path, parent_path, sizeof(parent_path));
    if (strlen(parent_path) == 0) {
        return -EINVAL; // No parent directory
    }

    fs_dir_t_init(&dir);
    rc = fs_opendir(&dir, parent_path);
    if (rc < 0) {
        return rc;
    }
    fs_closedir(&dir);
    return 0; // Path valid
}

static int cmd_fscmd_log(const struct shell *sh, size_t argc, char **argv) {

    if ((!strcmp(argv[1], "s") || !strcmp(argv[1], "p"))) {
        if (!is_valid_sensor_id(atoi(argv[2])) || !is_integer(argv[2])) {
            shell_error(sh, "Invalid Sensor ID");
            return -EINVAL;
        }
    }

    if (argc == 3 && !strcmp(argv[1], "p") && (output.did == atoi(argv[2]))) {
        output.start_logging = false;
        output.did = -1;
        output.file_path = NULL; 
    } else if (argc == 4 && !strcmp(argv[1], "s")) {
        output.start_logging = true;
        output.did = atoi(argv[2]);
        output.file_path = argv[3];

        struct fs_file_t file;
        int rc;

        rc = validate_file_path(output.file_path);
        if (rc < 0) {
            shell_error(sh, "Invalid Path: Parent Directory \"%s\" Not Found", output.file_path);
            return -ENOENT;
        }

        fs_file_t_init(&file);
        rc = fs_open(&file, output.file_path, FS_O_WRITE | FS_O_APPEND);
        if (rc < 0) {
            rc = fs_open(&file, output.file_path, FS_O_WRITE | FS_O_CREATE);
            if (rc < 0) {
                shell_error(sh, "File Path Not Exist");
            }
            shell_print(sh, "File created: %s", output.file_path);
        }
        fs_close(&file);

    } else {
        shell_error(sh, "Invalid Command");
        return -EINVAL;
    }

    shell_print(sh, "File Logging Active: %s, DID: %d, Path: %s", output.start_logging ? "ON" : "OFF", output.did, output.file_path);
    
    return 0;
}

/* Subcommand set */
SHELL_STATIC_SUBCMD_SET_CREATE(fscmd_subcmds,
    SHELL_CMD_ARG(create, NULL, "Create a file", cmd_fscmd_create, 2, 0),
    SHELL_CMD_ARG(log, NULL, "Start/Stop logging: log <s/p> <DID> <path>", cmd_fscmd_log, 3, 1),
    SHELL_SUBCMD_SET_END
);

/* Main fs command */
SHELL_CMD_REGISTER(fscmd, &fscmd_subcmds, "File System Commands", NULL);

int main(void) {
    
    rtc_init();
    init_button();

    SEGGER_RTT_Init();

    int rc;
    rc = fs_mount(&lfs_storage_mnt);
	if (rc < 0) {
		return 0;
	}
}