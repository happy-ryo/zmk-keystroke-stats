/*
 * Copyright (c) 2025 zmk-keystroke-stats contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zmk/keystroke_stats.h>

#if CONFIG_LVGL
#include <lvgl.h>
#endif

LOG_MODULE_REGISTER(keystroke_stats_prospector, CONFIG_ZMK_KEYSTROKE_STATS_LOG_LEVEL);

/**
 * @brief Prospector LVGL UI implementation
 *
 * This file provides a placeholder for the Prospector LVGL widget.
 * Full implementation will include:
 * - LVGL widget creation (horizontal layout, 220x48px)
 * - Number formatting (hybrid: 0-9999 as-is, 10K+ as "12.3K")
 * - Font rendering (FRAC_Regular_32 for numbers, FoundryGridnikMedium_20 for labels)
 * - Color highlighting (TODAY in cyan #00ffe5, others white)
 * - Auto-update on statistics changes
 *
 * TODO: Full implementation pending LVGL integration testing
 */

#if CONFIG_LVGL

static lv_obj_t *widget_container = NULL;
static lv_obj_t *label_today = NULL;
static lv_obj_t *label_yesterday = NULL;
static lv_obj_t *label_total = NULL;

static void update_display(const struct zmk_keystroke_stats *stats, void *user_data) {
    ARG_UNUSED(user_data);

    if (widget_container == NULL) {
        return;
    }

    /* TODO: Format numbers with hybrid approach */
    /* TODO: Update LVGL labels */

    LOG_DBG("Prospector UI updated: Today=%u, Yesterday=%u, Total=%u",
            stats->today_keystrokes,
            stats->yesterday_keystrokes,
            stats->total_keystrokes);
}

static int prospector_ui_init(void) {
    LOG_INF("Initializing Prospector LVGL UI");

    /* TODO: Create LVGL widgets */
    /* TODO: Set up layout (flexbox, horizontal) */
    /* TODO: Configure fonts and colors */

    /* Register callback */
    int ret = zmk_keystroke_stats_register_callback(update_display, NULL);
    if (ret < 0) {
        LOG_ERR("Failed to register callback: %d", ret);
        return ret;
    }

    LOG_WRN("Prospector UI initialized (PLACEHOLDER - full implementation pending)");

    return 0;
}

#else /* !CONFIG_LVGL */

static int prospector_ui_init(void) {
    LOG_ERR("Prospector UI requires LVGL, but CONFIG_LVGL is not enabled");
    return -ENOTSUP;
}

#endif /* CONFIG_LVGL */

KEYSTROKE_STATS_UI_DEFINE(prospector, prospector_ui_init, 90);
