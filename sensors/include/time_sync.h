#ifndef TIME_SYNC_LIB_H
#define TIME_SYNC_LIB_H

#include <stdint.h> // For uint32_t, etc.

// Define the maximum number of data points to keep for linear regression.
// A larger window provides more stable results but uses more memory.
#define TIME_SYNC_REGRESSION_WINDOW_SIZE 100

#define REGRESSION_CALC_INTERVAL_MS  2000 // How often to recalculate regression on sensor node

// --- Data Structures ---

// Structure to hold a single data point for regression.
// X typically represents the "master" clock's time, Y the "slave" clock's time.
typedef struct {
    uint32_t x_time; // Timestamp from the reference clock (e.g., MCU A's time)
    uint32_t y_time; // Timestamp from the local clock (e.g., MCU B's time)
} TimeSyncDataPoint_t;

// Structure to hold the current state of the time synchronization.
typedef struct {
    TimeSyncDataPoint_t data_points[TIME_SYNC_REGRESSION_WINDOW_SIZE];
    int data_point_count;
    int next_data_point_index; // For circular buffer management

    // Current regression parameters: y = slope * x + offset
    double slope;
    double offset;

    uint32_t last_regression_calc_time; // Local time when regression was last calculated
    uint32_t last_packet_send_time;     // Local time when last sync packet was sent

} TimeSyncState_t;

extern TimeSyncState_t sensor_node_time_sync_state;


// --- Function Prototypes ---

/**
 * @brief Initializes the time synchronization library state.
 * Should be called once at the start of your application.
 *
 * @param state A pointer to the TimeSyncState_t structure to initialize.
 */
void time_sync_init(TimeSyncState_t *state);

/**
 * @brief Adds a new data point to the regression model's internal buffer.
 * This function should be called whenever a relevant timestamp pair
 * (e.g., received remote time, local time at reception) is available.
 *
 * @param state A pointer to the current TimeSyncState_t.
 * @param x_time The timestamp from the reference clock (e.g., the time value received from the other MCU).
 * @param y_time The timestamp from the local clock (e.g., the local time when the remote timestamp was received).
 */
void time_sync_add_data_point(TimeSyncState_t *state, uint32_t x_time, uint32_t y_time);

/**
 * @brief Performs the linear regression calculation using the stored data points.
 * This function updates the 'slope' and 'offset' in the TimeSyncState_t.
 * It should be called periodically.
 *
 * @param state A pointer to the current TimeSyncState_t.
 */
void time_sync_calculate_regression(TimeSyncState_t *state);

/**
 * @brief Gets the calculated slope from the last regression.
 *
 * @param state A pointer to the current TimeSyncState_t.
 * @return The calculated slope (m) where Y = mX + c.
 */
double time_sync_get_slope(const TimeSyncState_t *state);

/**
 * @brief Gets the calculated offset from the last regression.
 *
 * @param state A pointer to the current TimeSyncState_t.
 * @return The calculated offset (c) where Y = mX + c.
 */
double time_sync_get_offset(const TimeSyncState_t *state);

/**
 * @brief Estimates the remote clock's time based on the local clock and the regression model.
 * This function uses the current local time and the calculated slope/offset
 * to estimate what the reference clock's time would be.
 *
 * @param state A pointer to the current TimeSyncState_t.
 * @param local_time_ms The current timestamp from the local microcontroller's clock.
 * @return An estimated time of the remote clock.
 */
double time_sync_estimate_remote_time(const TimeSyncState_t *state, uint32_t local_time_ms);

/**
 * @brief Estimates the local clock's time if it were synchronized to the remote clock.
 * This function uses the current remote time and the calculated slope/offset
 * to estimate what the local time should be.
 *
 * @param state A pointer to the current TimeSyncState_t.
 * @param remote_time_ms The current timestamp from the remote microcontroller's clock.
 * @return An estimated time of the local clock, if perfectly synced.
 */
double time_sync_estimate_local_synced_time(const TimeSyncState_t *state, uint32_t remote_time_ms);

void update_time_sync_regression();


#endif // TIME_SYNC_LIB_H