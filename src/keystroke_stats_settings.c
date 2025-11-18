/*
 * Copyright (c) 2025 zmk-keystroke-stats contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <zmk/keystroke_stats.h>

LOG_MODULE_REGISTER(keystroke_stats_settings, CONFIG_ZMK_KEYSTROKE_STATS_LOG_LEVEL);

/* Settings key prefix */
#define SETTINGS_KEY "keystroke_stats"

/* Current data structure version */
#define SETTINGS_VERSION 1

/**
 * @brief Persistent data structure
 *
 * This structure is saved to NVS and loaded at boot.
 * Version field allows for future migration.
 */
struct persisted_data {
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

/* External state access (from keystroke_stats.c) */
extern struct {
    uint32_t total_keystrokes;
    uint32_t today_keystrokes;
    uint32_t yesterday_keystrokes;

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_SESSION_TRACKING
    uint32_t session_keystrokes;
    uint32_t session_start_time;
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_WPM
    uint8_t current_wpm;
    uint8_t average_wpm;
    uint8_t peak_wpm;
    uint32_t total_typing_time_ms;
    struct {
        uint32_t keystrokes[10];
        uint32_t timestamps[10];
        uint8_t head;
        uint8_t count;
    } wpm_window;
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_KEY_HEATMAP
    uint32_t key_counts[CONFIG_ZMK_KEYSTROKE_STATS_MAX_KEY_POSITIONS];
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_DAILY_HISTORY
    struct zmk_keystroke_stats_daily_entry daily_history[CONFIG_ZMK_KEYSTROKE_STATS_DAILY_HISTORY_DAYS];
    uint8_t daily_history_count;
#endif

    uint16_t current_uptime_day;
    uint32_t last_keystroke_time;

    struct {
        zmk_keystroke_stats_callback_t callback;
        void *user_data;
    } callbacks[4];
    uint8_t callback_count;

    struct k_work_delayable save_work;
    bool save_pending;
    bool initialized;
} state;

/* External mutex */
extern struct k_mutex stats_mutex;

/**
 * @brief Settings load callback
 *
 * Called by Zephyr settings subsystem when loading persisted data.
 */
static int settings_load_handler(const char *key, size_t len,
                                  settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    size_t name_len;

    name_len = settings_name_next(key, &next);

    if (!next) {
        /* Root key: "keystroke_stats" */
        if (!strncmp(key, "data", name_len)) {
            struct persisted_data data;

            if (len != sizeof(data)) {
                LOG_ERR("Persisted data size mismatch: expected %zu, got %zu",
                        sizeof(data), len);
                return -EINVAL;
            }

            int rc = read_cb(cb_arg, &data, sizeof(data));
            if (rc < 0) {
                LOG_ERR("Failed to read settings: %d", rc);
                return rc;
            }

            /* Version check */
            if (data.version != SETTINGS_VERSION) {
                LOG_WRN("Settings version mismatch: %u != %u (ignoring)",
                        data.version, SETTINGS_VERSION);
                return 0;
            }

            /* Load data into state */
            k_mutex_lock(&stats_mutex, K_FOREVER);

            state.total_keystrokes = data.total_keystrokes;
            state.today_keystrokes = data.today_keystrokes;
            state.yesterday_keystrokes = data.yesterday_keystrokes;
            state.current_uptime_day = data.current_uptime_day;

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_WPM
            state.peak_wpm = data.peak_wpm;
            state.total_typing_time_ms = data.total_typing_time_ms;
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_KEY_HEATMAP
            memcpy(state.key_counts, data.key_counts, sizeof(state.key_counts));
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_DAILY_HISTORY
            memcpy(state.daily_history, data.daily_history, sizeof(state.daily_history));
            state.daily_history_count = data.daily_history_count;
#endif

            k_mutex_unlock(&stats_mutex);

            LOG_INF("Loaded persisted statistics:");
            LOG_INF("  Total keystrokes: %u", data.total_keystrokes);
            LOG_INF("  Today: %u, Yesterday: %u",
                    data.today_keystrokes, data.yesterday_keystrokes);
            LOG_INF("  Uptime day: %u", data.current_uptime_day);

            return 0;
        }
    }

    return -ENOENT;
}

/**
 * @brief Settings export callback
 *
 * Called by Zephyr settings subsystem when saving data.
 */
static int settings_export_handler(int (*cb)(const char *name,
                                              const void *value,
                                              size_t val_len)) {
    struct persisted_data data;

    k_mutex_lock(&stats_mutex, K_FOREVER);

    /* Prepare data structure */
    memset(&data, 0, sizeof(data));
    data.version = SETTINGS_VERSION;
    data.total_keystrokes = state.total_keystrokes;
    data.today_keystrokes = state.today_keystrokes;
    data.yesterday_keystrokes = state.yesterday_keystrokes;
    data.current_uptime_day = state.current_uptime_day;

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_WPM
    data.peak_wpm = state.peak_wpm;
    data.total_typing_time_ms = state.total_typing_time_ms;
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_KEY_HEATMAP
    memcpy(data.key_counts, state.key_counts, sizeof(data.key_counts));
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_DAILY_HISTORY
    memcpy(data.daily_history, state.daily_history, sizeof(data.daily_history));
    data.daily_history_count = state.daily_history_count;
#endif

    k_mutex_unlock(&stats_mutex);

    /* Save to settings */
    int rc = cb(SETTINGS_KEY "/data", &data, sizeof(data));
    if (rc < 0) {
        LOG_ERR("Failed to export settings: %d", rc);
        return rc;
    }

    LOG_DBG("Exported statistics to settings (%zu bytes)", sizeof(data));

    return 0;
}

/* Settings handler structure */
SETTINGS_STATIC_HANDLER_DEFINE(keystroke_stats_settings, SETTINGS_KEY,
                                NULL,  /* get */
                                settings_load_handler,
                                NULL,  /* commit */
                                settings_export_handler);

/**
 * @brief Save current statistics to persistent storage
 *
 * Called by keystroke_stats.c save work handler.
 */
int keystroke_stats_save_to_settings(void) {
    int rc = settings_save_one(SETTINGS_KEY "/data", NULL, 0);
    if (rc < 0) {
        LOG_ERR("Failed to save settings: %d", rc);
        return rc;
    }

    LOG_DBG("Statistics saved to persistent storage");
    return 0;
}

/**
 * @brief Load statistics from persistent storage
 *
 * Called during module initialization.
 */
int keystroke_stats_load_from_settings(void) {
    /* Settings framework automatically calls our load handler */
    int rc = settings_load_subtree(SETTINGS_KEY);
    if (rc < 0) {
        LOG_ERR("Failed to load settings: %d", rc);
        return rc;
    }

    return 0;
}

/**
 * @brief Initialize settings subsystem for keystroke stats
 */
static int keystroke_stats_settings_init(const struct device *dev) {
    ARG_UNUSED(dev);

    LOG_INF("Keystroke statistics settings module initialized");

    return 0;
}

SYS_INIT(keystroke_stats_settings_init, APPLICATION, 40);
