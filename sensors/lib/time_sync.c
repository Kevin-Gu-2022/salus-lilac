#include "time_sync.h"
#include <stddef.h> // For NULL
#include <string.h> // For memset
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(time_sync, LOG_LEVEL_DBG);

// --- Internal Defines and Constants ---
// Minimum number of points required to perform a regression calculation.
// Setting this too low can lead to unstable results.
#define MIN_POINTS_FOR_REGRESSION 5

// X in the data points will be the Base Unit's time.
// Y in the data points will be the Sensor Node's local time.
TimeSyncState_t sensor_node_time_sync_state;

// --- Function Implementations ---

void time_sync_init(TimeSyncState_t *state) {
    if (state == NULL) {
        return; // Handle null pointer gracefully
    }
    memset(state, 0, sizeof(TimeSyncState_t)); // Clear the state structure
    state->slope = 1.0; // Initialize slope to 1 (assuming clocks run at same rate initially)
    state->offset = 0.0; // Initialize offset to 0 (assuming no initial difference)
}

void time_sync_add_data_point(TimeSyncState_t *state, uint32_t x_time, uint32_t y_time) {
    if (state == NULL) {
        return;
    }

    state->data_points[state->next_data_point_index].x_time = x_time;
    state->data_points[state->next_data_point_index].y_time = y_time;

    state->next_data_point_index = (state->next_data_point_index + 1) % TIME_SYNC_REGRESSION_WINDOW_SIZE;

    if (state->data_point_count < TIME_SYNC_REGRESSION_WINDOW_SIZE) {
        state->data_point_count++;
    }
}

void time_sync_calculate_regression(TimeSyncState_t *state) {
    if (state == NULL || state->data_point_count < MIN_POINTS_FOR_REGRESSION) {
        // Not enough data points to perform meaningful regression.
        // Keep existing slope/offset or default values.
        return;
    }

    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_xy = 0.0;
    double sum_xx = 0.0;
    double n = (double)state->data_point_count;

    for (int i = 0; i < state->data_point_count; i++) {
        sum_x += (double)state->data_points[i].x_time;
        sum_y += (double)state->data_points[i].y_time;
        sum_xy += (double)state->data_points[i].x_time * (double)state->data_points[i].y_time;
        sum_xx += (double)state->data_points[i].x_time * (double)state->data_points[i].x_time;
    }

    // Calculate slope (m)
    double denominator = (n * sum_xx - sum_x * sum_x);
    if (denominator != 0.0) { // Avoid division by zero
        state->slope = (n * sum_xy - sum_x * sum_y) / denominator;
    } else {
        // If denominator is zero, all X values are the same, slope is undefined or vertical.
        // In this context, it usually means insufficient variation in timestamps or error.
        // We'll keep the previous slope or default to 1.0.
        state->slope = 1.0; // Default to 1.0 to prevent bad values
    }

    // Calculate offset (c)
    state->offset = (sum_y - state->slope * sum_x) / n;

    // Update the last calculation time
    // Note: The caller needs to provide the current time to set this accurately.
    // For simplicity, we'll assume the main loop manages this.
    // In a real system, you might pass current_local_time_ms to this function.
    // state->last_regression_calc_time = current_local_time_ms;
}

double time_sync_get_slope(const TimeSyncState_t *state) {
    if (state == NULL) {
        return 1.0; // Default if state is invalid
    }
    return state->slope;
}

double time_sync_get_offset(const TimeSyncState_t *state) {
    if (state == NULL) {
        return 0.0; // Default if state is invalid
    }
    return state->offset;
}

double time_sync_estimate_remote_time(const TimeSyncState_t *state, uint32_t local_time_ms) {
    if (state == NULL) {
        return (double)local_time_ms; // If no sync, assume times are same
    }
    // We have: Y_local = slope * X_remote + offset
    // We want: X_remote = (Y_local - offset) / slope
    return ((double)local_time_ms - state->offset) / state->slope;
}

double time_sync_estimate_local_synced_time(const TimeSyncState_t *state, uint32_t remote_time_ms) {
    if (state == NULL) {
        return (double)remote_time_ms; // If no sync, assume times are same
    }
    // We have: Y_local = slope * X_remote + offset
    return state->slope * (double)remote_time_ms + state->offset;
}

uint32_t get_synchronized_time_ms() {
    uint32_t local_raw_time = (uint32_t) k_uptime_get();

    // Use the library function to estimate what the Base Unit's time would be
    // based on our local raw time.
    double estimated_base_unit_time = time_sync_estimate_remote_time(&sensor_node_time_sync_state, local_raw_time);

    // Convert the double result back to uint32_t.
    // Be mindful of potential rounding or casting issues for very large time values.
    return (uint32_t)estimated_base_unit_time;
}

void update_time_sync_regression() {

    uint32_t current_sensor_local_time = (uint32_t) k_uptime_get();

    // Periodically recalculate the linear regression
    if (current_sensor_local_time - sensor_node_time_sync_state.last_regression_calc_time >= REGRESSION_CALC_INTERVAL_MS) {
        time_sync_calculate_regression(&sensor_node_time_sync_state);
        sensor_node_time_sync_state.last_regression_calc_time = current_sensor_local_time;

        // Optional: Print or log results for debugging
        double slope = time_sync_get_slope(&sensor_node_time_sync_state);
        double offset = time_sync_get_offset(&sensor_node_time_sync_state);
        LOG_INF("Sensor Node: Slope: %.6f, Offset: %.2f\n", slope, offset);
    }

    // --- Example Usage of Synchronized Time ---
    uint32_t synced_time = get_synchronized_time_ms();
    LOG_INF("Synced time: %d", synced_time);
}
