/*
 * Copyright (c) 2025 zmk-keystroke-stats contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zmk/keystroke_stats.h>

LOG_MODULE_REGISTER(keystroke_stats_oled, CONFIG_ZMK_KEYSTROKE_STATS_LOG_LEVEL);

/**
 * @brief OLED SSD1306 UI implementation
 *
 * This file provides a placeholder for the OLED display implementation.
 * Full implementation will include:
 * - SSD1306 display driver integration (128x64px monochrome)
 * - Text rendering with built-in fonts
 * - Vertical layout for statistics display:
 *   Line 1: "TODAY: 1234"
 *   Line 2: "YESTERDAY: 987"
 *   Line 3: "TOTAL: 12.3K"
 *   Line 4: "WPM: 45" (if enabled)
 * - Power-efficient updates (2-second interval default)
 *
 * TODO: Full implementation pending OLED hardware testing
 */

#if CONFIG_DISPLAY

static const struct device *display_dev = NULL;
static struct k_timer update_timer;

static void update_display(const struct zmk_keystroke_stats *stats, void *user_data) {
    ARG_UNUSED(user_data);

    if (display_dev == NULL) {
        return;
    }

    /* TODO: Clear display buffer */
    /* TODO: Render statistics text */
    /* TODO: Push buffer to display */

    LOG_DBG("OLED UI updated: Today=%u, Yesterday=%u, Total=%u",
            stats->today_keystrokes,
            stats->yesterday_keystrokes,
            stats->total_keystrokes);
}

static void timer_handler(struct k_timer *timer) {
    ARG_UNUSED(timer);

    /* Get current stats and update display */
    struct zmk_keystroke_stats stats;
    if (zmk_keystroke_stats_get(&stats) == 0) {
        update_display(&stats, NULL);
    }
}

static int oled_ui_init(void) {
    LOG_INF("Initializing OLED UI");

    /* TODO: Get display device */
    /* display_dev = DEVICE_DT_GET(...); */

    /* TODO: Initialize display */
    /* TODO: Clear display */

    /* Register callback */
    int ret = zmk_keystroke_stats_register_callback(update_display, NULL);
    if (ret < 0) {
        LOG_ERR("Failed to register callback: %d", ret);
        return ret;
    }

    /* Start periodic update timer */
    k_timer_init(&update_timer, timer_handler, NULL);
    k_timer_start(&update_timer,
                  K_MSEC(CONFIG_ZMK_KEYSTROKE_STATS_OLED_UPDATE_INTERVAL_MS),
                  K_MSEC(CONFIG_ZMK_KEYSTROKE_STATS_OLED_UPDATE_INTERVAL_MS));

    LOG_WRN("OLED UI initialized (PLACEHOLDER - full implementation pending)");

    return 0;
}

#else /* !CONFIG_DISPLAY */

static int oled_ui_init(void) {
    LOG_ERR("OLED UI requires display driver, but CONFIG_DISPLAY is not enabled");
    return -ENOTSUP;
}

#endif /* CONFIG_DISPLAY */

KEYSTROKE_STATS_UI_DEFINE(oled, oled_ui_init, 90);
