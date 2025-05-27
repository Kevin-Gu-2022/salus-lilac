#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <stdbool.h>

#define ROW_COUNT 4
#define COL_COUNT 4

#define ROW_ALIAS(i) DT_ALIAS(row##i)
#define COL_ALIAS(i) DT_ALIAS(col##i)
#define GPIO_SPEC(alias) GPIO_DT_SPEC_GET(alias, gpios)

// Rows: output
static const struct gpio_dt_spec rows[ROW_COUNT] = {
    GPIO_SPEC(ROW_ALIAS(0)),
    GPIO_SPEC(ROW_ALIAS(1)),
    GPIO_SPEC(ROW_ALIAS(2)),
    GPIO_SPEC(ROW_ALIAS(3)),
};

// Columns: input with pull-up
static const struct gpio_dt_spec cols[COL_COUNT] = {
    GPIO_SPEC(COL_ALIAS(0)),
    GPIO_SPEC(COL_ALIAS(1)),
    GPIO_SPEC(COL_ALIAS(2)),
    GPIO_SPEC(COL_ALIAS(3)),
};

static const char keymap[ROW_COUNT][COL_COUNT] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'0', 'F', 'E', 'D'}
};

static bool prev_state[ROW_COUNT][COL_COUNT] = {0};

void main(void)
{
    printk("Initializing keypad...\n");

    for (int i = 0; i < ROW_COUNT; i++) {
        if (!device_is_ready(rows[i].port)) {
            printk("Row %d not ready\n", i);
            return;
        }
        gpio_pin_configure_dt(&rows[i], GPIO_OUTPUT_INACTIVE);
        gpio_pin_set_dt(&rows[i], 0);  // start LOW
    }

    for (int i = 0; i < COL_COUNT; i++) {
        if (!device_is_ready(cols[i].port)) {
            printk("Col %d not ready\n", i);
            return;
        }
        gpio_pin_configure_dt(&cols[i], GPIO_INPUT | GPIO_PULL_UP);
    }

    printk("Keypad ready. Scanning...\n");

    bool scanning_ready = false;

    while (1) {
        for (int r = 0; r < ROW_COUNT; r++) {
            for (int i = 0; i < ROW_COUNT; i++) {
                gpio_pin_set_dt(&rows[i], 0);
            }
    
            gpio_pin_set_dt(&rows[r], 1);
            k_busy_wait(100);
    
            for (int c = 0; c < COL_COUNT; c++) {
                bool pressed = gpio_pin_get_dt(&cols[c]) == 0;
    
                if (pressed && !prev_state[r][c]) {
                    if (scanning_ready) {
                        printk("Button %c is pressed\n", keymap[r][c]);
                    }
                    prev_state[r][c] = true;
                } else if (!pressed && prev_state[r][c]) {
                    prev_state[r][c] = false;
                }
            }
    
            gpio_pin_set_dt(&rows[r], 0);
        }
    
        // Enable printing after one clean scan
        if (!scanning_ready) {
            scanning_ready = true;
        }
    
        k_msleep(50);
    }
    
}
