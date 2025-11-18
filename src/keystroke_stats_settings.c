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

/* Note: struct zmk_keystroke_stats_persist_data is now defined in the public header.
 * This matches the layout of the old 'struct persisted_data'.
 * We use the public API functions zmk_keystroke_stats_get_persist_data() and
 * zmk_keystroke_stats_load_persist_data() to access the internal state.
 */

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
            struct zmk_keystroke_stats_persist_data data;

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

            /* Version check and load via public API */
            if (data.version != SETTINGS_VERSION) {
                LOG_WRN("Settings version mismatch: %u != %u (ignoring)",
                        data.version, SETTINGS_VERSION);
                return 0;
            }

            /* Use public API to load data (provides mutex protection) */
            rc = zmk_keystroke_stats_load_persist_data(&data);
            if (rc < 0) {
                LOG_ERR("Failed to load persist data: %d", rc);
                return rc;
            }

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
    struct zmk_keystroke_stats_persist_data data;

    /* Use public API to get data (provides mutex protection) */
    int rc = zmk_keystroke_stats_get_persist_data(&data);
    if (rc < 0) {
        LOG_ERR("Failed to get persist data: %d", rc);
        return rc;
    }

    /* Save to settings */
    rc = cb(SETTINGS_KEY "/data", &data, sizeof(data));
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
