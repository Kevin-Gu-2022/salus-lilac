#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>

// Servo device and pulse width limits from Devicetree
static const struct pwm_dt_spec servo = PWM_DT_SPEC_GET(DT_NODELABEL(servo));
static const uint32_t min_pulse = DT_PROP(DT_NODELABEL(servo), min_pulse);
static const uint32_t max_pulse = DT_PROP(DT_NODELABEL(servo), max_pulse);

// Direction enum
enum direction {
	DOWN,
	UP,
};

// Global direction state
static enum direction dir = UP;

// Toggle servo between min and max pulse widths
void unlock_or_lock(void) {
    uint32_t pulse_width = (dir == UP) ? max_pulse : min_pulse;

    int ret = pwm_set_pulse_dt(&servo, pulse_width);
    if (ret < 0) {
        printk("Error %d: failed to set pulse width\n", ret);
        return;
    }

    printk("Moved to %s (pulse: %d us)\n", dir == UP ? "180°" : "0°", pulse_width);

    // Flip direction
    dir = (dir == UP) ? DOWN : UP;
}

int main(void) {
	printk("Servomotor control\n");

	if (!pwm_is_ready_dt(&servo)) {
		printk("Error: PWM device %s is not ready\n", servo.dev->name);
		return 0;
	}

	// Initialize to 0°
    pwm_set_pulse_dt(&servo, min_pulse);
    dir = UP;

    while (1) {
        k_sleep(K_SECONDS(5));
        unlock_or_lock();
    }

	return 0;
}
