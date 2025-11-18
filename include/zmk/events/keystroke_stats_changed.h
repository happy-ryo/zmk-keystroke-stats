/*
 * Copyright (c) 2025 zmk-keystroke-stats contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/kernel.h>
#include <zmk/event_manager.h>

/**
 * @brief Event raised when keystroke statistics are updated
 *
 * This event is raised periodically (not on every keystroke) to notify
 * UI components that they should update their display of keystroke statistics.
 *
 * The event contains the current statistics values for efficient display updates.
 */
struct zmk_keystroke_stats_changed {
    /** Total keystrokes across all time */
    uint32_t total_keystrokes;
    /** Today's keystroke count */
    uint32_t today_keystrokes;
    /** Yesterday's keystroke count */
    uint32_t yesterday_keystrokes;
};

ZMK_EVENT_DECLARE(zmk_keystroke_stats_changed);
