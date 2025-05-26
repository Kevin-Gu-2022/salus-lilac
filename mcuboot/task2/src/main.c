#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#define PRIORITY                1      // Priority for both threads
#define NUM_OF_COLOURS          8      // Number of color states
#define NUM_OF_BITS             8      // Number of bits per byte
#define NUM_OF_BYTES            4      // Number of bytes
#define STACKSIZE               1024   // Stack size for threads
#define MSGQ_SIZE               10     // Message queue size
#define ALIGNMENT_SIZE_IN_BYTES 4      // Alignment size for message queue

// GPIO device tree alias definitions
#define DATA_NODE DT_ALIAS(data)       // Data pin alias
#define CLOCK_NODE DT_ALIAS(clock)     // Clock pin alias

// GPIO structures for controlling RGB LED
static const struct gpio_dt_spec rgb_data = GPIO_DT_SPEC_GET(DATA_NODE, gpios);
static const struct gpio_dt_spec rgb_clock = GPIO_DT_SPEC_GET(CLOCK_NODE, gpios);

// Synchronization primitives
K_SEM_DEFINE(signal_sem, 1, 1); 
K_MSGQ_DEFINE(rgb_msgq, sizeof(uint32_t), MSGQ_SIZE, ALIGNMENT_SIZE_IN_BYTES);

/*
 * Initializes the GPIO pins for RGB LED control.
 */
void rgb_init(void) {

    int ret;

    // Configure the data pin as an output
    ret = gpio_pin_configure_dt(&rgb_data, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error configuring rgb_data: %d\n", ret);
    }

    // Configure the clock pin as an output
    ret = gpio_pin_configure_dt(&rgb_clock, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error configuring rgb_clock: %d\n", ret);
    }
}

/*
 * Generates a clock pulse for data transmission.
 */
void clk(void) {

    gpio_pin_set_dt(&rgb_clock, 0);
    k_busy_wait(1);  // Short delay
    gpio_pin_set_dt(&rgb_clock, 1);
    k_busy_wait(1);  // Short delay
}

/*
 * Sends a byte of data to the RGB LED.
 */
void send_byte(uint8_t byte) {

    for (uint8_t i = 0; i < NUM_OF_BITS; i++) {

        // Set data line based on MSB
        gpio_pin_set_dt(&rgb_data, (byte & 0x80) != 0);
        clk();       // Generate clock pulse
        byte <<= 1;  // Shift left to send the next bit
    }
}

/*
 * Sends an RGB color value with brightness adjustment.
 */
void send_rgb_colour(uint8_t red, uint8_t green, uint8_t blue, float brightness) {

    uint8_t prefix = 0b11000000;

    // Encoding color brightness control bits
    if ((blue & 0x80) == 0)     prefix |= 0b00100000;
    if ((blue & 0x40) == 0)     prefix |= 0b00010000; 
    if ((green & 0x80) == 0)    prefix |= 0b00001000;
    if ((green & 0x40) == 0)    prefix |= 0b00000100;
    if ((red & 0x80) == 0)      prefix |= 0b00000010;
    if ((red & 0x40) == 0)      prefix |= 0b00000001;

    send_byte(prefix);
    send_byte((uint8_t)(blue * brightness));
    send_byte((uint8_t)(green * brightness));
    send_byte((uint8_t)(red * brightness));
}

/*
 * Sets the RGB LED color based on a predefined value.
 */
void set_rgb_colour(uint8_t rgb_value, float brightness) {

    // Start frame (4 zero bytes)
    for (int i = 0; i < NUM_OF_BYTES; i++) {
        send_byte(0x00);
    }

    // Extract RGB values from the 3-bit input
    uint8_t red = ((rgb_value >> 2) & 1) * 255;
    uint8_t green = ((rgb_value >> 1) & 1) * 255;
    uint8_t blue = (rgb_value & 1) * 255;

    send_rgb_colour(red, green, blue, brightness);

    // End frame (4 zero bytes)
    for (int i = 0; i < NUM_OF_BYTES; i++) {
        send_byte(0x00);
    }
}

/*
 * RGB thread: Cycles through different colors and sends them to the message queue.
 */
void rgb_thread(void *, void *, void *) {

    uint8_t color_num = 7;

    while (1) {

        // Wait for signal to update color
        k_sem_take(&signal_sem, K_FOREVER);   

        // Cycle through 8 colors
        color_num = (color_num + 1) % NUM_OF_COLOURS;

        if (k_msgq_put(&rgb_msgq, &color_num, K_FOREVER) != 0) {
            // Handle message queue put failure
        }
    }
}

/*
 * Control thread: Receives color data and updates the LED.
 */
void control_thread(void *, void *, void *) {

    uint8_t color_num = 0;
    rgb_init();  // Initialize GPIOs

    while (1) {

        k_msgq_get(&rgb_msgq, &color_num, K_FOREVER);  // Wait for color data
        set_rgb_colour(color_num, 0.5);                // Update LED with 50% brightness
        k_sleep(K_SECONDS(2));                         // Wait for 2 seconds before changing color
        k_sem_give(&signal_sem);                       // Signal RGB thread to generate the next color
    }
}

int main(void) {

    return 0;   // Main does nothing since threads are defined outside
}

// Define and start threads
K_THREAD_DEFINE(control_tid, STACKSIZE, rgb_thread, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(set_tid, STACKSIZE, control_thread, NULL, NULL, NULL, PRIORITY, 0, 0);