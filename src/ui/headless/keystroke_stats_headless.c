/*
 * Copyright (c) 2025 zmk-keystroke-stats contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/keystroke_stats.h>

LOG_MODULE_REGISTER(keystroke_stats_headless, CONFIG_ZMK_KEYSTROKE_STATS_LOG_LEVEL);

/**
 * @brief Headless UI implementation
 *
 * This "UI" implementation does nothing visually but provides logging
 * of statistics for debugging purposes. It's useful for:
 * - Testing the core statistics engine without display hardware
 * - Debugging via UART/RTT logs
 * - Headless keyboard builds that access stats via other means
 */

static void stats_callback(const struct zmk_keystroke_stats *stats, void *user_data) {
    ARG_UNUSED(user_data);

    /* Log statistics periodically */
    LOG_INF("=== Keystroke Statistics ===");
    LOG_INF("Today: %u, Yesterday: %u, Total: %u",
            stats->today_keystrokes,
            stats->yesterday_keystrokes,
            stats->total_keystrokes);

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_SESSION_TRACKING
    LOG_INF("Session: %u keystrokes", stats->session_keystrokes);
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_WPM
    LOG_INF("WPM - Current: %u, Average: %u, Peak: %u",
            stats->current_wpm,
            stats->average_wpm,
            stats->peak_wpm);
#endif

#if CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_KEY_HEATMAP
    LOG_INF("Top 3 keys:");
    for (int i = 0; i < 3 && i < CONFIG_ZMK_KEYSTROKE_STATS_TOP_KEYS_COUNT; i++) {
        if (stats->top_keys[i].count > 0) {
            LOG_INF("  #%d: Position %u = %u presses",
                    i + 1,
                    stats->top_keys[i].position,
                    stats->top_keys[i].count);
        }
    }
#endif

    LOG_INF("===========================");
}

static int headless_ui_init(void) {
    LOG_INF("Initializing headless UI (logging only)");

    /* Register callback for statistics updates */
    int ret = zmk_keystroke_stats_register_callback(stats_callback, NULL);
    if (ret < 0) {
        LOG_ERR("Failed to register callback: %d", ret);
        return ret;
    }

    LOG_INF("Headless UI initialized - statistics will be logged");

    return 0;
}

/* Register UI implementation using the macro from public API */
KEYSTROKE_STATS_UI_DEFINE(headless, headless_ui_init, 90);
