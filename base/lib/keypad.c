/*
* @file     keypad.c
* @brief    Keypad Library
* @author   Xinlin Zhong, 48018061
*/

#include "keypad.h"

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

void keypad_init(void) {
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
}

// Returns pressed key char if new key pressed, else '\0'
char keypad_scan(void) {
    static bool scanning_ready = false; // retains its value between calls
    
    for (int r = 0; r < ROW_COUNT; r++) {
        // Reset all rows low
        for (int i = 0; i < ROW_COUNT; i++) {
            gpio_pin_set_dt(&rows[i], 0);
        }

        // Set current row high
        gpio_pin_set_dt(&rows[r], 1);
        k_busy_wait(100);

        for (int c = 0; c < COL_COUNT; c++) {
            bool pressed = (gpio_pin_get_dt(&cols[c]) == 0);

            if (pressed && !prev_state[r][c]) {
                prev_state[r][c] = true;
                if (scanning_ready) {
                    // Return the detected key immediately
                    gpio_pin_set_dt(&rows[r], 0);  // Reset row before returning
                    return keymap[r][c];
                }
            } else if (!pressed && prev_state[r][c]) {
                prev_state[r][c] = false;
            }
        }

        gpio_pin_set_dt(&rows[r], 0);
    }

    if (!scanning_ready) {
        scanning_ready = true;  // Enable after first scan
    }

    return '\0';  // No new key pressed
}