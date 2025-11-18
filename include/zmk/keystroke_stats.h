/*
 * Copyright (c) 2025 zmk-keystroke-stats contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file keystroke_stats.h
 * @brief Public API for ZMK Keystroke Statistics module
 *
 * This module tracks keystroke statistics with persistent storage across
 * firmware updates. Statistics include today's count, yesterday's count,
 * total count, WPM tracking, key usage heatmaps, and daily history.
 */

/**
 * @brief Maximum number of top keys tracked in statistics
 *
 * This is controlled by CONFIG_ZMK_KEYSTROKE_STATS_TOP_KEYS_COUNT
 */
#define ZMK_KEYSTROKE_STATS_MAX_TOP_KEYS \
    CONFIG_ZMK_KEYSTROKE_STATS_TOP_KEYS_COUNT

/**
 * @brief Maximum number of days in daily history
 *
 * This is controlled by CONFIG_ZMK_KEYSTROKE_STATS_DAILY_HISTORY_DAYS
 */
#define ZMK_KEYSTROKE_STATS_MAX_HISTORY_DAYS \
    CONFIG_ZMK_KEYSTROKE_STATS_DAILY_HISTORY_DAYS

/**
 * @brief Key usage entry for heatmap tracking
 */
struct zmk_keystroke_stats_key_entry {
    /** Key position index */
    uint32_t position;
    /** Number of times this key was pressed */
    uint32_t count;
};

/**
 * @brief Daily statistics entry
 */
struct zmk_keystroke_stats_daily_entry {
    /** Year (since epoch, or uptime-based identifier) */
    uint16_t year;
    /** Month (1-12, or 0 if uptime-based) */
    uint8_t month;
    /** Day (1-31, or uptime day counter) */
    uint8_t day;
    /** Number of keystrokes on this day */
    uint32_t keystrokes;
};

/**
 * @brief Complete keystroke statistics structure
 *
 * This structure contains all tracked statistics. Use zmk_keystroke_stats_get()
 * to populate this structure with current data.
 */
struct zmk_keystroke_stats {
    /** Total keystrokes across all time (persists across firmware updates) */
    uint32_t total_keystrokes;

    /** Today's keystroke count (resets at day rollover) */
    uint32_t today_keystrokes;

    /** Yesterday's keystroke count */
    uint32_t yesterday_keystrokes;

    /** Current session keystrokes (resets after inactivity timeout) */
    uint32_t session_keystrokes;

    /** Current words per minute (if WPM tracking enabled) */
    uint8_t current_wpm;

    /** Average WPM for current session */
    uint8_t average_wpm;

    /** Peak WPM achieved in current session */
    uint8_t peak_wpm;

    /** Total typing time in milliseconds (active typing, not idle time) */
    uint32_t total_typing_time_ms;

    /** Session start timestamp (k_uptime_get()) */
    uint32_t session_start_time;

    /** Last keystroke timestamp (k_uptime_get()) */
    uint32_t last_keystroke_time;

    /** Top most-pressed keys (if heatmap enabled) */
    struct zmk_keystroke_stats_key_entry top_keys[ZMK_KEYSTROKE_STATS_MAX_TOP_KEYS];

    /** Daily statistics history (if daily history enabled) */
    struct zmk_keystroke_stats_daily_entry daily_stats[ZMK_KEYSTROKE_STATS_MAX_HISTORY_DAYS];

    /** Number of valid entries in daily_stats array */
    uint8_t daily_stats_count;

    /** Current uptime day (for day rollover tracking) */
    uint16_t current_uptime_day;
};

/**
 * @brief Callback function type for statistics updates
 *
 * Register a callback to be notified when statistics change.
 *
 * @param stats Pointer to current statistics
 * @param user_data User data passed during registration
 */
typedef void (*zmk_keystroke_stats_callback_t)(const struct zmk_keystroke_stats *stats,
                                                 void *user_data);

/**
 * @brief Get current keystroke statistics
 *
 * Populates the provided structure with current statistics data.
 *
 * @param stats Pointer to structure to populate
 * @return 0 on success, negative errno on failure
 */
int zmk_keystroke_stats_get(struct zmk_keystroke_stats *stats);

/**
 * @brief Get keystroke count for a specific key position
 *
 * Only available if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_KEY_HEATMAP is enabled.
 *
 * @param position Key position index
 * @param count Pointer to store the count
 * @return 0 on success, -ENOTSUP if heatmap disabled, -EINVAL if position invalid
 */
int zmk_keystroke_stats_get_key_count(uint32_t position, uint32_t *count);

/**
 * @brief Manually trigger save to persistent storage
 *
 * Normally statistics are saved automatically at the configured interval.
 * This function forces an immediate save.
 *
 * Note: A debounce delay (CONFIG_ZMK_KEYSTROKE_STATS_SAVE_DEBOUNCE_MS)
 * is applied before the actual write to prevent flash wear.
 *
 * @return 0 on success (save scheduled), negative errno on failure
 */
int zmk_keystroke_stats_save(void);

/**
 * @brief Reset statistics
 *
 * @param reset_total If true, resets total_keystrokes as well.
 *                    If false, only resets today/yesterday/session stats.
 * @return 0 on success, negative errno on failure
 */
int zmk_keystroke_stats_reset(bool reset_total);

/**
 * @brief Register a callback for statistics updates
 *
 * The callback will be invoked whenever statistics change significantly
 * (e.g., keystroke count increments, WPM updates, day rollover).
 *
 * @param callback Callback function
 * @param user_data User data to pass to callback
 * @return 0 on success, negative errno on failure
 */
int zmk_keystroke_stats_register_callback(zmk_keystroke_stats_callback_t callback,
                                            void *user_data);

/**
 * @brief Unregister a previously registered callback
 *
 * @param callback Callback function to unregister
 * @return 0 on success, -ENOENT if not found
 */
int zmk_keystroke_stats_unregister_callback(zmk_keystroke_stats_callback_t callback);

/**
 * @brief Persistent data structure for settings storage
 *
 * This structure contains all fields that are persisted to non-volatile storage.
 * The settings layer uses the accessor functions below to get/set this data.
 *
 * Version field allows for future data migration.
 */
struct zmk_keystroke_stats_persist_data {
    uint8_t version;
    uint32_t total_keystrokes;
    uint32_t today_keystrokes;
    uint32_t yesterday_keystrokes;
    uint16_t current_uptime_day;

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_WPM
    uint8_t peak_wpm;
    uint32_t total_typing_time_ms;
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_KEY_HEATMAP
    uint32_t key_counts[CONFIG_ZMK_KEYSTROKE_STATS_MAX_KEY_POSITIONS];
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_DAILY_HISTORY
    struct zmk_keystroke_stats_daily_entry daily_history[CONFIG_ZMK_KEYSTROKE_STATS_DAILY_HISTORY_DAYS];
    uint8_t daily_history_count;
#endif
} __packed;

/**
 * @brief Get persistent data for settings storage
 *
 * This function is used by the settings layer to retrieve data that should
 * be persisted to non-volatile storage. The function is thread-safe and
 * uses mutex protection internally.
 *
 * @param data Pointer to structure to populate with persistent data
 * @return 0 on success, negative errno on failure
 */
int zmk_keystroke_stats_get_persist_data(struct zmk_keystroke_stats_persist_data *data);

/**
 * @brief Load persistent data from settings storage
 *
 * This function is used by the settings layer to restore previously saved data.
 * The function validates the data version and is thread-safe.
 *
 * @param data Pointer to persistent data to load
 * @return 0 on success, negative errno on failure (-EINVAL for invalid version)
 */
int zmk_keystroke_stats_load_persist_data(const struct zmk_keystroke_stats_persist_data *data);

/**
 * @brief Macro for defining a keystroke statistics UI implementation
 *
 * UI implementations should use this macro to register themselves with
 * the keystroke statistics system.
 *
 * Example:
 * @code
 * static int my_ui_init(void) {
 *     // Initialize UI
 *     return 0;
 * }
 *
 * KEYSTROKE_STATS_UI_DEFINE(my_ui, my_ui_init, 50);
 * @endcode
 *
 * @param _name Unique identifier for this UI implementation
 * @param _init_fn Initialization function (int (*)(void))
 * @param _priority Initialization priority (lower runs first)
 */
#define KEYSTROKE_STATS_UI_DEFINE(_name, _init_fn, _priority)                                   \
    static int _keystroke_stats_ui_init_##_name(const struct device *dev) {                     \
        ARG_UNUSED(dev);                                                                         \
        return _init_fn();                                                                       \
    }                                                                                            \
    SYS_INIT(_keystroke_stats_ui_init_##_name, APPLICATION, _priority)

#ifdef __cplusplus
}
#endif
