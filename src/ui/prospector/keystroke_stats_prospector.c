/*
 * Copyright (c) 2025 zmk-keystroke-stats contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zmk/keystroke_stats.h>
#include <stdio.h>

#if CONFIG_LVGL
#include <lvgl.h>
#include <prospector_screen.h>
#endif

LOG_MODULE_REGISTER(keystroke_stats_prospector, CONFIG_ZMK_KEYSTROKE_STATS_LOG_LEVEL);

/**
 * @brief Prospector LVGL UI implementation
 *
 * This provides a horizontal widget displaying keystroke statistics:
 * - Layout: 220x48px horizontal flexbox
 * - Font: FRAC_Regular_32 for numbers, FoundryGridnikMedium_20 for labels
 * - Colors: TODAY in cyan (#00ffe5), others in white
 * - Number formatting: Hybrid (0-9999 as-is, 10K+ as "12.3K")
 */

#if CONFIG_LVGL

/* External font declarations from Prospector module */
LV_FONT_DECLARE(FRAC_Regular_32);
LV_FONT_DECLARE(FoundryGridnikMedium_20);

static lv_obj_t *widget_container = NULL;
static lv_obj_t *label_today_num = NULL;
static lv_obj_t *label_today_text = NULL;
static lv_obj_t *label_yesterday_num = NULL;
static lv_obj_t *label_yesterday_text = NULL;
static lv_obj_t *label_total_num = NULL;
static lv_obj_t *label_total_text = NULL;

/**
 * @brief Format number with hybrid approach
 *
 * - 0-9999: Display as-is (e.g., "1234")
 * - 10000+: Display with K suffix (e.g., "12.3K")
 */
static void format_number(uint32_t value, char *buf, size_t buf_size) {
    if (value < 10000) {
        snprintf(buf, buf_size, "%u", value);
    } else {
        uint32_t thousands = value / 1000;
        uint32_t remainder = (value % 1000) / 100;

        if (remainder == 0) {
            snprintf(buf, buf_size, "%uK", thousands);
        } else {
            snprintf(buf, buf_size, "%u.%uK", thousands, remainder);
        }
    }
}

static void update_display(const struct zmk_keystroke_stats *stats, void *user_data) {
    ARG_UNUSED(user_data);

    if (widget_container == NULL || label_today_num == NULL) {
        return;
    }

    char buf[16];

    /* Update TODAY */
    format_number(stats->today_keystrokes, buf, sizeof(buf));
    lv_label_set_text(label_today_num, buf);

    /* Update YESTERDAY */
    format_number(stats->yesterday_keystrokes, buf, sizeof(buf));
    lv_label_set_text(label_yesterday_num, buf);

    /* Update TOTAL */
    format_number(stats->total_keystrokes, buf, sizeof(buf));
    lv_label_set_text(label_total_num, buf);

    LOG_DBG("Prospector UI updated: Today=%u, Yesterday=%u, Total=%u",
            stats->today_keystrokes,
            stats->yesterday_keystrokes,
            stats->total_keystrokes);
}

/**
 * @brief Create a single stat column (number + label)
 */
static void create_stat_column(lv_obj_t *parent, const char *label_text,
                               lv_obj_t **out_num, lv_obj_t **out_text,
                               bool is_highlighted) {
    /* Container for this stat */
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_style_bg_opa(col, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(col, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(col, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Number label */
    lv_obj_t *num = lv_label_create(col);
    lv_obj_set_style_text_font(num, &FRAC_Regular_32, 0);
    if (is_highlighted) {
        lv_obj_set_style_text_color(num, lv_color_hex(0x00ffe5), 0);  /* Cyan for TODAY */
    } else {
        lv_obj_set_style_text_color(num, lv_color_white(), 0);
    }
    lv_label_set_text(num, "0");
    lv_obj_align(num, LV_ALIGN_CENTER, 0, -6);

    /* Text label */
    lv_obj_t *text = lv_label_create(col);
    lv_obj_set_style_text_font(text, &FoundryGridnikMedium_20, 0);
    lv_obj_set_style_text_color(text, lv_color_hex(0x808080), 0);  /* Gray */
    lv_label_set_text(text, label_text);
    lv_obj_align(text, LV_ALIGN_CENTER, 0, 10);

    *out_num = num;
    *out_text = text;
}

static int prospector_ui_init(void) {
    LOG_INF("Initializing Prospector LVGL UI");

    /* Get the Prospector screen */
    lv_obj_t *screen = prospector_get_screen();
    if (screen == NULL) {
        LOG_ERR("Failed to get Prospector screen - may not be initialized yet");
        return -ENODEV;
    }

    /* Create main container widget - positioned at bottom above battery bar */
    widget_container = lv_obj_create(screen);
    lv_obj_set_size(widget_container, 220, 48);
    lv_obj_align(widget_container, LV_ALIGN_BOTTOM_MID, 0, -48);  /* Above battery bar (48px height) */
    lv_obj_set_style_bg_opa(widget_container, 0, LV_PART_MAIN);  /* Transparent background */
    lv_obj_set_style_border_width(widget_container, 0, LV_PART_MAIN);  /* No border */
    lv_obj_set_style_pad_all(widget_container, 2, LV_PART_MAIN);
    lv_obj_set_flex_flow(widget_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(widget_container,
                         LV_FLEX_ALIGN_SPACE_EVENLY,  /* Main axis (horizontal) */
                         LV_FLEX_ALIGN_CENTER,         /* Cross axis (vertical) */
                         LV_FLEX_ALIGN_CENTER);        /* Track alignment */

    /* Create three columns: TODAY, YESTERDAY, TOTAL */
    create_stat_column(widget_container, "TODAY",
                      &label_today_num, &label_today_text, true);

    create_stat_column(widget_container, "YESTERDAY",
                      &label_yesterday_num, &label_yesterday_text, false);

    create_stat_column(widget_container, "TOTAL",
                      &label_total_num, &label_total_text, false);

    /* Register callback for statistics updates */
    int ret = zmk_keystroke_stats_register_callback(update_display, NULL);
    if (ret < 0) {
        LOG_ERR("Failed to register callback: %d", ret);
        return ret;
    }

    /* Initial update with current stats */
    const struct zmk_keystroke_stats *stats = zmk_keystroke_stats_get_current();
    if (stats != NULL) {
        update_display(stats, NULL);
    }

    LOG_INF("Prospector UI initialized successfully");
    return 0;
}

#else /* !CONFIG_LVGL */

static int prospector_ui_init(void) {
    LOG_ERR("Prospector UI requires LVGL, but CONFIG_LVGL is not enabled");
    return -ENOTSUP;
}

#endif /* CONFIG_LVGL */

// DISABLED: UI is integrated directly into Prospector's zmk_display_status_screen()
// SYS_INIT(prospector_ui_init, APPLICATION, 92);
