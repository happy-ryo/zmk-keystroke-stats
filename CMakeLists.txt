# Copyright (c) 2025 zmk-keystroke-stats contributors
# SPDX-License-Identifier: MIT

if(CONFIG_ZMK_KEYSTROKE_STATS)

# Core statistics engine
zephyr_library()
zephyr_library_sources(src/keystroke_stats.c)
zephyr_library_sources(src/keystroke_stats_settings.c)
zephyr_library_sources(src/events/keystroke_stats_changed.c)
zephyr_library_include_directories(include)

# UI implementation selection
if(CONFIG_ZMK_KEYSTROKE_STATS_UI_PROSPECTOR)
  zephyr_library_sources(src/ui/prospector/keystroke_stats_prospector.c)
  message(STATUS "ZMK Keystroke Stats: Using Prospector LVGL UI")
endif()

if(CONFIG_ZMK_KEYSTROKE_STATS_UI_OLED)
  zephyr_library_sources(src/ui/oled/keystroke_stats_oled.c)
  message(STATUS "ZMK Keystroke Stats: Using OLED SSD1306 UI")
endif()

if(CONFIG_ZMK_KEYSTROKE_STATS_UI_NONE)
  zephyr_library_sources(src/ui/headless/keystroke_stats_headless.c)
  message(STATUS "ZMK Keystroke Stats: Using headless mode (no UI)")
endif()

# Log configuration
if(CONFIG_ZMK_KEYSTROKE_STATS_LOG_LEVEL GREATER 0)
  message(STATUS "ZMK Keystroke Stats: Log level = ${CONFIG_ZMK_KEYSTROKE_STATS_LOG_LEVEL}")
endif()

# Feature status messages
if(CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_WPM)
  message(STATUS "ZMK Keystroke Stats: WPM tracking enabled")
endif()

if(CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_KEY_HEATMAP)
  message(STATUS "ZMK Keystroke Stats: Key heatmap tracking enabled (${CONFIG_ZMK_KEYSTROKE_STATS_MAX_KEY_POSITIONS} positions)")
endif()

if(CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_DAILY_HISTORY)
  message(STATUS "ZMK Keystroke Stats: Daily history enabled (${CONFIG_ZMK_KEYSTROKE_STATS_DAILY_HISTORY_DAYS} days)")
endif()

if(CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_SESSION_TRACKING)
  message(STATUS "ZMK Keystroke Stats: Session tracking enabled")
endif()

# Save interval warning
math(EXPR SAVE_INTERVAL_HOURS "${CONFIG_ZMK_KEYSTROKE_STATS_SAVE_INTERVAL_MS} / 3600000")
math(EXPR FLASH_LIFESPAN_YEARS "10000 * ${CONFIG_ZMK_KEYSTROKE_STATS_SAVE_INTERVAL_MS} / 31536000000")

if(SAVE_INTERVAL_HOURS LESS 3)
  message(WARNING "ZMK Keystroke Stats: Save interval is ${SAVE_INTERVAL_HOURS}h, flash lifespan ~${FLASH_LIFESPAN_YEARS} years. Consider increasing save interval.")
else()
  message(STATUS "ZMK Keystroke Stats: Save interval = ${SAVE_INTERVAL_HOURS}h, estimated flash lifespan ~${FLASH_LIFESPAN_YEARS} years")
endif()

endif() # CONFIG_ZMK_KEYSTROKE_STATS
