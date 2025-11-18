/*
 * Copyright (c) 2025 zmk-keystroke-stats contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/keystroke_stats.h>

LOG_MODULE_REGISTER(zmk_keystroke_stats, CONFIG_ZMK_KEYSTROKE_STATS_LOG_LEVEL);

/* Forward declarations */
static void update_wpm(void);
static void check_day_rollover(void);
static void notify_callbacks(void);
static void schedule_save(void);

/* Internal state structure */
static struct {
    /* Core statistics */
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

    /* WPM calculation window */
    struct {
        uint32_t keystrokes[10];  /* Ring buffer of keystroke counts */
        uint32_t timestamps[10];  /* Corresponding timestamps */
        uint8_t head;             /* Next write position */
        uint8_t count;            /* Number of valid entries */
    } wpm_window;
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_KEY_HEATMAP
    uint32_t key_counts[CONFIG_ZMK_KEYSTROKE_STATS_MAX_KEY_POSITIONS];
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_DAILY_HISTORY
    struct zmk_keystroke_stats_daily_entry daily_history[CONFIG_ZMK_KEYSTROKE_STATS_DAILY_HISTORY_DAYS];
    uint8_t daily_history_count;
#endif

    /* Day tracking (uptime-based) */
    uint16_t current_uptime_day;
    uint32_t last_keystroke_time;

    /* Callback system */
    struct {
        zmk_keystroke_stats_callback_t callback;
        void *user_data;
    } callbacks[4];  /* Support up to 4 registered callbacks */
    uint8_t callback_count;

    /* Save management */
    struct k_work_delayable save_work;
    bool save_pending;
    bool initialized;

} state = {
    .initialized = false,
    .callback_count = 0,
};

/* Mutex for thread-safe access */
K_MUTEX_DEFINE(stats_mutex);

/**
 * @brief Get current uptime day
 *
 * Calculates which "day" we're in based on uptime and configured rollover hour.
 * Day 0 = first 24 hours, Day 1 = next 24 hours, etc.
 */
static uint16_t get_uptime_day(void) {
    uint32_t uptime_ms = k_uptime_get();
    uint32_t uptime_hours = uptime_ms / 3600000;

    /* Adjust for rollover hour */
    int32_t adjusted_hours = uptime_hours - CONFIG_ZMK_KEYSTROKE_STATS_DAY_ROLLOVER_HOUR;
    if (adjusted_hours < 0) {
        adjusted_hours = 0;
    }

    return (uint16_t)(adjusted_hours / 24);
}

/**
 * @brief Check if day has rolled over and update statistics
 */
static void check_day_rollover(void) {
    uint16_t current_day = get_uptime_day();

    if (current_day != state.current_uptime_day) {
        LOG_INF("Day rollover detected: day %u -> %u",
                state.current_uptime_day, current_day);

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_DAILY_HISTORY
        /* Add yesterday to history */
        if (state.daily_history_count < CONFIG_ZMK_KEYSTROKE_STATS_DAILY_HISTORY_DAYS) {
            state.daily_history[state.daily_history_count].year = 0;  /* Uptime-based */
            state.daily_history[state.daily_history_count].month = 0;
            state.daily_history[state.daily_history_count].day = (uint8_t)state.current_uptime_day;
            state.daily_history[state.daily_history_count].keystrokes = state.today_keystrokes;
            state.daily_history_count++;
        } else {
            /* Shift history and add new entry */
            for (int i = 0; i < CONFIG_ZMK_KEYSTROKE_STATS_DAILY_HISTORY_DAYS - 1; i++) {
                state.daily_history[i] = state.daily_history[i + 1];
            }
            state.daily_history[CONFIG_ZMK_KEYSTROKE_STATS_DAILY_HISTORY_DAYS - 1].year = 0;
            state.daily_history[CONFIG_ZMK_KEYSTROKE_STATS_DAILY_HISTORY_DAYS - 1].month = 0;
            state.daily_history[CONFIG_ZMK_KEYSTROKE_STATS_DAILY_HISTORY_DAYS - 1].day = (uint8_t)state.current_uptime_day;
            state.daily_history[CONFIG_ZMK_KEYSTROKE_STATS_DAILY_HISTORY_DAYS - 1].keystrokes = state.today_keystrokes;
        }
#endif

        /* Roll over stats */
        state.yesterday_keystrokes = state.today_keystrokes;
        state.today_keystrokes = 0;
        state.current_uptime_day = current_day;

        /* Trigger save and notify */
        schedule_save();
        notify_callbacks();
    }
}

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_WPM
/**
 * @brief Update WPM calculation
 *
 * Uses a sliding window approach to calculate current WPM.
 * Standard WPM: (keystrokes / 5) / (time in minutes)
 */
static void update_wpm(void) {
    uint32_t now = k_uptime_get();
    uint32_t window_ms = CONFIG_ZMK_KEYSTROKE_STATS_WPM_WINDOW_MS;

    /* Add current keystroke to window */
    state.wpm_window.timestamps[state.wpm_window.head] = now;
    state.wpm_window.keystrokes[state.wpm_window.head] = 1;
    state.wpm_window.head = (state.wpm_window.head + 1) % 10;
    if (state.wpm_window.count < 10) {
        state.wpm_window.count++;
    }

    /* Calculate WPM based on window */
    uint32_t total_keystrokes = 0;
    uint32_t oldest_time = now;

    for (int i = 0; i < state.wpm_window.count; i++) {
        if (now - state.wpm_window.timestamps[i] <= window_ms) {
            total_keystrokes += state.wpm_window.keystrokes[i];
            if (state.wpm_window.timestamps[i] < oldest_time) {
                oldest_time = state.wpm_window.timestamps[i];
            }
        }
    }

    uint32_t elapsed_ms = now - oldest_time;
    if (elapsed_ms > 0 && total_keystrokes > 0) {
        /* WPM = (keystrokes / 5) / (time in minutes) */
        uint32_t wpm = (total_keystrokes * 60000) / (elapsed_ms * 5);
        state.current_wpm = (uint8_t)MIN(wpm, 255);

        /* Update peak */
        if (state.current_wpm > state.peak_wpm) {
            state.peak_wpm = state.current_wpm;
        }
    } else {
        state.current_wpm = 0;
    }

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_SESSION_TRACKING
    /* Update average WPM for session */
    if (state.session_keystrokes > 0 && state.session_start_time > 0) {
        uint32_t session_duration_ms = now - state.session_start_time;
        if (session_duration_ms > 0) {
            uint32_t avg = (state.session_keystrokes * 60000) / (session_duration_ms * 5);
            state.average_wpm = (uint8_t)MIN(avg, 255);
        }
    }
#endif
}
#endif /* CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_WPM */

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_SESSION_TRACKING
/**
 * @brief Check if session should be reset due to inactivity
 */
static void check_session_timeout(void) {
    uint32_t now = k_uptime_get();
    uint32_t idle_time = now - state.last_keystroke_time;

    if (idle_time > CONFIG_ZMK_KEYSTROKE_STATS_SESSION_TIMEOUT_MS) {
        LOG_INF("Session timeout, resetting session stats");
        state.session_keystrokes = 0;
        state.session_start_time = now;

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_WPM
        state.average_wpm = 0;
        state.peak_wpm = 0;
        state.wpm_window.count = 0;
        state.wpm_window.head = 0;
#endif
    }
}
#endif /* CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_SESSION_TRACKING */

/**
 * @brief Notify all registered callbacks
 */
static void notify_callbacks(void) {
    if (state.callback_count == 0) {
        return;
    }

    struct zmk_keystroke_stats stats;
    if (zmk_keystroke_stats_get(&stats) == 0) {
        for (int i = 0; i < state.callback_count; i++) {
            if (state.callbacks[i].callback != NULL) {
                state.callbacks[i].callback(&stats, state.callbacks[i].user_data);
            }
        }
    }
}

/**
 * @brief Work handler for delayed save
 */
static void save_work_handler(struct k_work *work) {
    /* This will be implemented in keystroke_stats_settings.c */
    extern int keystroke_stats_save_to_settings(void);

    int ret = keystroke_stats_save_to_settings();
    if (ret == 0) {
        LOG_INF("Statistics saved to persistent storage");
        state.save_pending = false;
    } else {
        LOG_ERR("Failed to save statistics: %d", ret);
    }
}

/**
 * @brief Schedule a save operation (with debounce)
 */
static void schedule_save(void) {
    if (!state.initialized) {
        return;
    }

    /* Cancel any pending save and reschedule */
    k_work_cancel_delayable(&state.save_work);
    k_work_schedule(&state.save_work,
                    K_MSEC(CONFIG_ZMK_KEYSTROKE_STATS_SAVE_DEBOUNCE_MS));
    state.save_pending = true;

    LOG_DBG("Save scheduled in %d ms", CONFIG_ZMK_KEYSTROKE_STATS_SAVE_DEBOUNCE_MS);
}

/**
 * @brief Handle keystroke events
 */
static int keystroke_event_listener(const zmk_event_t *eh) {
    struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    /* Only count key presses, not releases */
    if (!ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    k_mutex_lock(&stats_mutex, K_FOREVER);

    /* Update counts */
    state.total_keystrokes++;
    state.today_keystrokes++;

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_SESSION_TRACKING
    check_session_timeout();

    if (state.session_keystrokes == 0) {
        state.session_start_time = k_uptime_get();
    }
    state.session_keystrokes++;
#endif

    state.last_keystroke_time = k_uptime_get();

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_KEY_HEATMAP
    /* Update key heatmap */
    uint32_t position = ev->usage_page;  /* TODO: Use actual key position */
    if (position < CONFIG_ZMK_KEYSTROKE_STATS_MAX_KEY_POSITIONS) {
        state.key_counts[position]++;
    }
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_WPM
    update_wpm();
#endif

    /* Check for day rollover */
    check_day_rollover();

    /* Notify callbacks */
    notify_callbacks();

    k_mutex_unlock(&stats_mutex);

    LOG_DBG("Keystroke recorded: total=%u, today=%u",
            state.total_keystrokes, state.today_keystrokes);

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(keystroke_stats, keystroke_event_listener);
ZMK_SUBSCRIPTION(keystroke_stats, zmk_keycode_state_changed);

/* Periodic save timer */
static void periodic_save_handler(struct k_timer *timer) {
    LOG_INF("Periodic save triggered");
    schedule_save();
}

K_TIMER_DEFINE(periodic_save_timer, periodic_save_handler, NULL);

/**
 * @brief Public API Implementation
 */

int zmk_keystroke_stats_get(struct zmk_keystroke_stats *stats) {
    if (stats == NULL) {
        return -EINVAL;
    }

    k_mutex_lock(&stats_mutex, K_FOREVER);

    memset(stats, 0, sizeof(*stats));

    stats->total_keystrokes = state.total_keystrokes;
    stats->today_keystrokes = state.today_keystrokes;
    stats->yesterday_keystrokes = state.yesterday_keystrokes;
    stats->last_keystroke_time = state.last_keystroke_time;
    stats->current_uptime_day = state.current_uptime_day;

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_SESSION_TRACKING
    stats->session_keystrokes = state.session_keystrokes;
    stats->session_start_time = state.session_start_time;
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_WPM
    stats->current_wpm = state.current_wpm;
    stats->average_wpm = state.average_wpm;
    stats->peak_wpm = state.peak_wpm;
    stats->total_typing_time_ms = state.total_typing_time_ms;
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_KEY_HEATMAP
    /* Find top N keys */
    struct {
        uint32_t position;
        uint32_t count;
    } top_keys[CONFIG_ZMK_KEYSTROKE_STATS_MAX_KEY_POSITIONS];

    /* Copy all keys */
    for (int i = 0; i < CONFIG_ZMK_KEYSTROKE_STATS_MAX_KEY_POSITIONS; i++) {
        top_keys[i].position = i;
        top_keys[i].count = state.key_counts[i];
    }

    /* Simple bubble sort to find top N */
    for (int i = 0; i < CONFIG_ZMK_KEYSTROKE_STATS_TOP_KEYS_COUNT; i++) {
        for (int j = i + 1; j < CONFIG_ZMK_KEYSTROKE_STATS_MAX_KEY_POSITIONS; j++) {
            if (top_keys[j].count > top_keys[i].count) {
                /* Swap */
                uint32_t temp_pos = top_keys[i].position;
                uint32_t temp_count = top_keys[i].count;
                top_keys[i].position = top_keys[j].position;
                top_keys[i].count = top_keys[j].count;
                top_keys[j].position = temp_pos;
                top_keys[j].count = temp_count;
            }
        }
        stats->top_keys[i].position = top_keys[i].position;
        stats->top_keys[i].count = top_keys[i].count;
    }
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_DAILY_HISTORY
    stats->daily_stats_count = state.daily_history_count;
    for (int i = 0; i < state.daily_history_count; i++) {
        stats->daily_stats[i] = state.daily_history[i];
    }
#endif

    k_mutex_unlock(&stats_mutex);

    return 0;
}

int zmk_keystroke_stats_get_key_count(uint32_t position, uint32_t *count) {
#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_KEY_HEATMAP
    if (count == NULL) {
        return -EINVAL;
    }

    if (position >= CONFIG_ZMK_KEYSTROKE_STATS_MAX_KEY_POSITIONS) {
        return -EINVAL;
    }

    k_mutex_lock(&stats_mutex, K_FOREVER);
    *count = state.key_counts[position];
    k_mutex_unlock(&stats_mutex);

    return 0;
#else
    return -ENOTSUP;
#endif
}

int zmk_keystroke_stats_save(void) {
    schedule_save();
    return 0;
}

int zmk_keystroke_stats_reset(bool reset_total) {
    k_mutex_lock(&stats_mutex, K_FOREVER);

    LOG_WRN("Resetting statistics (reset_total=%d)", reset_total);

    if (reset_total) {
        state.total_keystrokes = 0;
    }

    state.today_keystrokes = 0;
    state.yesterday_keystrokes = 0;

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_SESSION_TRACKING
    state.session_keystrokes = 0;
    state.session_start_time = k_uptime_get();
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_WPM
    state.current_wpm = 0;
    state.average_wpm = 0;
    state.peak_wpm = 0;
    state.wpm_window.count = 0;
    state.wpm_window.head = 0;
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_KEY_HEATMAP
    memset(state.key_counts, 0, sizeof(state.key_counts));
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_DAILY_HISTORY
    state.daily_history_count = 0;
    memset(state.daily_history, 0, sizeof(state.daily_history));
#endif

    k_mutex_unlock(&stats_mutex);

    schedule_save();
    notify_callbacks();

    return 0;
}

int zmk_keystroke_stats_register_callback(zmk_keystroke_stats_callback_t callback,
                                           void *user_data) {
    if (callback == NULL) {
        return -EINVAL;
    }

    k_mutex_lock(&stats_mutex, K_FOREVER);

    if (state.callback_count >= ARRAY_SIZE(state.callbacks)) {
        k_mutex_unlock(&stats_mutex);
        return -ENOMEM;
    }

    state.callbacks[state.callback_count].callback = callback;
    state.callbacks[state.callback_count].user_data = user_data;
    state.callback_count++;

    k_mutex_unlock(&stats_mutex);

    LOG_INF("Callback registered (%d total)", state.callback_count);

    return 0;
}

int zmk_keystroke_stats_unregister_callback(zmk_keystroke_stats_callback_t callback) {
    k_mutex_lock(&stats_mutex, K_FOREVER);

    for (int i = 0; i < state.callback_count; i++) {
        if (state.callbacks[i].callback == callback) {
            /* Shift remaining callbacks */
            for (int j = i; j < state.callback_count - 1; j++) {
                state.callbacks[j] = state.callbacks[j + 1];
            }
            state.callback_count--;
            k_mutex_unlock(&stats_mutex);
            LOG_INF("Callback unregistered");
            return 0;
        }
    }

    k_mutex_unlock(&stats_mutex);
    return -ENOENT;
}

/**
 * @brief Module initialization
 */
static int keystroke_stats_init(const struct device *dev) {
    ARG_UNUSED(dev);

    LOG_INF("Initializing keystroke statistics module");

    /* Initialize state */
    memset(&state, 0, sizeof(state));
    state.current_uptime_day = get_uptime_day();

    /* Initialize delayed work for save */
    k_work_init_delayable(&state.save_work, save_work_handler);

    /* Load persisted data */
    extern int keystroke_stats_load_from_settings(void);
    int ret = keystroke_stats_load_from_settings();
    if (ret != 0) {
        LOG_WRN("Failed to load persisted statistics: %d (starting fresh)", ret);
    }

    /* Start periodic save timer */
    k_timer_start(&periodic_save_timer,
                  K_MSEC(CONFIG_ZMK_KEYSTROKE_STATS_SAVE_INTERVAL_MS),
                  K_MSEC(CONFIG_ZMK_KEYSTROKE_STATS_SAVE_INTERVAL_MS));

    state.initialized = true;

    LOG_INF("Keystroke statistics module initialized");
    LOG_INF("  Save interval: %d ms (%d hours)",
            CONFIG_ZMK_KEYSTROKE_STATS_SAVE_INTERVAL_MS,
            CONFIG_ZMK_KEYSTROKE_STATS_SAVE_INTERVAL_MS / 3600000);
    LOG_INF("  Current uptime day: %u", state.current_uptime_day);

    return 0;
}

SYS_INIT(keystroke_stats_init, APPLICATION, 50);

/* Persistence API implementation */

#define PERSIST_DATA_VERSION 1

int zmk_keystroke_stats_get_persist_data(struct zmk_keystroke_stats_persist_data *data) {
    if (!data) {
        return -EINVAL;
    }

    k_mutex_lock(&stats_mutex, K_FOREVER);

    /* Populate persistent data structure */
    data->version = PERSIST_DATA_VERSION;
    data->total_keystrokes = state.total_keystrokes;
    data->today_keystrokes = state.today_keystrokes;
    data->yesterday_keystrokes = state.yesterday_keystrokes;
    data->current_uptime_day = state.current_uptime_day;

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_WPM
    data->peak_wpm = state.peak_wpm;
    data->total_typing_time_ms = state.total_typing_time_ms;
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_KEY_HEATMAP
    memcpy(data->key_counts, state.key_counts, sizeof(data->key_counts));
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_DAILY_HISTORY
    memcpy(data->daily_history, state.daily_history, sizeof(data->daily_history));
    data->daily_history_count = state.daily_history_count;
#endif

    k_mutex_unlock(&stats_mutex);

    LOG_DBG("Persist data retrieved: version=%d, total=%u, today=%u",
            data->version, data->total_keystrokes, data->today_keystrokes);

    return 0;
}

int zmk_keystroke_stats_load_persist_data(const struct zmk_keystroke_stats_persist_data *data) {
    if (!data) {
        return -EINVAL;
    }

    /* Validate data version */
    if (data->version != PERSIST_DATA_VERSION) {
        LOG_WRN("Incompatible persist data version: %d (expected %d)",
                data->version, PERSIST_DATA_VERSION);
        return -EINVAL;
    }

    k_mutex_lock(&stats_mutex, K_FOREVER);

    /* Restore persistent fields */
    state.total_keystrokes = data->total_keystrokes;
    state.today_keystrokes = data->today_keystrokes;
    state.yesterday_keystrokes = data->yesterday_keystrokes;
    state.current_uptime_day = data->current_uptime_day;

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_WPM
    state.peak_wpm = data->peak_wpm;
    state.total_typing_time_ms = data->total_typing_time_ms;
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_KEY_HEATMAP
    memcpy(state.key_counts, data->key_counts, sizeof(state.key_counts));
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_DAILY_HISTORY
    memcpy(state.daily_history, data->daily_history, sizeof(state.daily_history));
    state.daily_history_count = data->daily_history_count;
#endif

    k_mutex_unlock(&stats_mutex);

    LOG_INF("Persist data loaded: total=%u, today=%u, yesterday=%u",
            state.total_keystrokes, state.today_keystrokes, state.yesterday_keystrokes);

    /* Notify callbacks about loaded data */
    notify_callbacks();

    return 0;
}
