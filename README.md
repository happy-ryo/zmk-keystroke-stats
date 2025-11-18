# ZMK Keystroke Statistics

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A ZMK module for tracking keystroke statistics with persistent storage across firmware updates.

## Features

- **Persistent Statistics**: Data survives firmware updates using Zephyr Settings API
- **Daily Tracking**: Today's keystrokes, yesterday's keystrokes, and total count
- **WPM Tracking**: Real-time words-per-minute calculation (optional)
- **Key Heatmap**: Per-key usage tracking for analyzing typing patterns (optional)
- **Multiple UI Options**:
  - Prospector LVGL widget (ST7789V 240x280px displays)
  - OLED SSD1306 support (128x64px monochrome)
  - Headless mode (API-only, no display)
- **Flash-Friendly**: Configurable save intervals to maximize flash lifespan
  - Default 24h interval = 27 year flash lifespan
- **Session Tracking**: Separate statistics for current typing session (optional)
- **Daily History**: Keep track of keystroke trends over time (optional)

## Quick Start

### 1. Add to your ZMK project

Add this module to your `config/west.yml`:

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: keystroke-stats
      url-base: https://github.com/YOUR_USERNAME  # Update with your username
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: zmk-keystroke-stats
      remote: keystroke-stats
      revision: main
  self:
    path: config
```

### 2. Enable in your keyboard config

Add to your board's `.conf` file (e.g., `kobito_left.conf`):

```conf
# Enable keystroke statistics
CONFIG_ZMK_KEYSTROKE_STATS=y

# Choose UI implementation (pick one)
CONFIG_ZMK_KEYSTROKE_STATS_UI_PROSPECTOR=y  # For Prospector LVGL
# CONFIG_ZMK_KEYSTROKE_STATS_UI_OLED=y      # For OLED displays
# CONFIG_ZMK_KEYSTROKE_STATS_UI_NONE=y      # Headless mode

# Optional: Enable WPM tracking
CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_WPM=y

# Optional: Enable key heatmap
CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_KEY_HEATMAP=y

# Optional: Save interval (default: 24 hours)
CONFIG_ZMK_KEYSTROKE_STATS_SAVE_INTERVAL_MS=86400000
```

### 3. Build and flash

```bash
west build -b your_board
```

## Configuration Options

### Core Settings

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_ZMK_KEYSTROKE_STATS` | `n` | Enable keystroke statistics module |
| `CONFIG_ZMK_KEYSTROKE_STATS_SAVE_INTERVAL_MS` | `86400000` | Save interval (24h default) |
| `CONFIG_ZMK_KEYSTROKE_STATS_SAVE_DEBOUNCE_MS` | `60000` | Debounce delay before writing |
| `CONFIG_ZMK_KEYSTROKE_STATS_DAY_ROLLOVER_HOUR` | `0` | Hour to roll over to next day |

### Features

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_WPM` | `y` | Enable WPM tracking |
| `CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_KEY_HEATMAP` | `y` | Per-key usage tracking |
| `CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_DAILY_HISTORY` | `y` | Keep daily history (7 days) |
| `CONFIG_ZMK_KEYSTROKE_STATS_ENABLE_SESSION_TRACKING` | `y` | Track current session separately |

### UI Selection

Choose **one** of:
- `CONFIG_ZMK_KEYSTROKE_STATS_UI_PROSPECTOR` - Prospector LVGL widget
- `CONFIG_ZMK_KEYSTROKE_STATS_UI_OLED` - OLED SSD1306 display
- `CONFIG_ZMK_KEYSTROKE_STATS_UI_NONE` - Headless mode

See [Kconfig](Kconfig) for complete list of options.

## Flash Endurance

This module is designed to be flash-friendly. The default 24-hour save interval provides approximately **27 years of flash lifespan** (assuming 10,000 write cycles).

| Save Interval | Flash Lifespan |
|---------------|----------------|
| 1 hour | 1.1 years |
| 6 hours | 6.8 years |
| 12 hours | 13.7 years |
| **24 hours** | **27.4 years** ⭐ |
| 48 hours | 54.8 years |

The module also uses a 60-second debounce delay to prevent excessive writes from multiple rapid changes.

## API Usage

### C API

```c
#include <zmk/keystroke_stats.h>

// Get current statistics
struct zmk_keystroke_stats stats;
int ret = zmk_keystroke_stats_get(&stats);

// Access data
printf("Today: %u\n", stats.today_keystrokes);
printf("Yesterday: %u\n", stats.yesterday_keystrokes);
printf("Total: %u\n", stats.total_keystrokes);
printf("Current WPM: %u\n", stats.current_wpm);

// Manually trigger save
zmk_keystroke_stats_save();

// Reset statistics
zmk_keystroke_stats_reset(false);  // Keep total
zmk_keystroke_stats_reset(true);   // Reset everything
```

See [include/zmk/keystroke_stats.h](include/zmk/keystroke_stats.h) for complete API documentation.

## Development Status

- [x] Project structure and build system
- [ ] Core statistics engine
- [ ] Settings API integration
- [ ] Prospector UI implementation
- [ ] OLED UI implementation
- [ ] Headless mode
- [ ] Unit tests
- [ ] Integration tests
- [ ] Documentation
- [ ] CI/CD pipeline

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Acknowledgments

- [ZMK Firmware](https://zmk.dev/) - The awesome mechanical keyboard firmware
- [Prospector](https://github.com/ebastler/Prospector) - Display module for ZMK
- Inspired by typing statistics trackers like WhatPulse

## Related Projects

- [zmk-rgbled-widget](https://github.com/caksoylar/zmk-rgbled-widget) - RGB LED status widget
- [zmk-pmw3610-driver](https://github.com/badjeff/zmk-pmw3610-driver) - Pointing device driver
- [prospector](https://github.com/ebastler/prospector) - Display module for ZMK
