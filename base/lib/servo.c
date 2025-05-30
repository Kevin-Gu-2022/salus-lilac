/*
* @file     servo.c
* @brief    Servo Motor Library
* @author   Xinlin Zhong, 48018061
*/

#include "servo.h"

LOG_MODULE_REGISTER(servo, LOG_LEVEL_DBG);

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

void servo_init(void) {
	if (!pwm_is_ready_dt(&servo)) {
		LOG_ERR("Error: PWM device %s is not ready", servo.dev->name);
		return;
	}

    LOG_INF("Servomotor initialised.");

	// Initialize to 0°
    pwm_set_pulse_dt(&servo, max_pulse);
    dir = UP;
}


// Toggle servo between min and max pulse widths
void servo_toggle(void) {
    uint32_t pulse_width = (dir == UP) ? max_pulse : min_pulse;

    int ret = pwm_set_pulse_dt(&servo, pulse_width);
    if (ret < 0) {
        LOG_ERR("Error %d: failed to set pulse width", ret);
        return;
    }
    // Flip direction
    dir = (dir == UP) ? DOWN : UP;
}