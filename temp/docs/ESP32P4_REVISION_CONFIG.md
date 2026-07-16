# ESP32-P4 Revision Configuration

[中文版本](./ESP32P4_REVISION_CONFIG_CN.md)

ESP32-P4 chips earlier than rev v3.0 and chips at rev v3.0 or later are not
binary compatible in ESP-IDF. A firmware image must be built for one side of
that boundary.

The examples in this repository do not hard-code the early engineering-sample
revision in their base `sdkconfig.defaults` files. Use one of the repository
overlays below when you need to pin the build to a specific chip revision
family.

## Which Profile Should I Use?

| Chip on your board | Recommended overlay | Notes |
| --- | --- | --- |
| ESP32-P4 rev v3.1 or later | `../../../config/esp32p4_rev_v3_1.defaults` | Matches the ESP-IDF v5.5 default minimum revision. |
| ESP32-P4 rev v3.0 or later | `../../../config/esp32p4_rev_v3_0.defaults` | Use this when production hardware includes v3.0 chips. |
| ESP32-P4 rev v0.x or v1.x engineering sample | `../../../config/esp32p4_rev_pre_v3.defaults` | Supports pre-v3 chips only. It will not support v3.x chips. |

If you do not pass a revision overlay, ESP-IDF chooses its own default. In
ESP-IDF v5.5, that default is rev v3.1 or later for normal builds.

## Build Commands

Run commands from the selected ESP-IDF example directory.

Build for production chips at rev v3.1 or later:

```bash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;../../../config/esp32p4_rev_v3_1.defaults" set-target esp32p4 build
```

Build for production chips at rev v3.0 or later:

```bash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;../../../config/esp32p4_rev_v3_0.defaults" set-target esp32p4 build
```

Build for earlier engineering samples:

```bash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;../../../config/esp32p4_rev_pre_v3.defaults" set-target esp32p4 build
```

ESP-IDF automatically appends target-specific defaults such as
`sdkconfig.defaults.esp32p4` when the base `sdkconfig.defaults` file is listed.
This keeps examples such as `17_simple_video_server` working with the same
command shape.

When switching an existing example directory from one revision family to
another, remove the local generated `sdkconfig` first or use a separate
`SDKCONFIG` file for each profile. Existing `sdkconfig` choices take precedence
over defaults and can otherwise hide the overlay you intended to test.

## Why This Matters

`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` selects ESP32-P4 revisions earlier than
v3.0 and excludes v3.x chips. In ESP-IDF, that choice affects low-level SoC
support such as eFuse layout, PMU and power settings, sleep support, PSRAM,
flash timing, and other hardware-dependent code paths.

For mass production, choose the minimum revision that matches the oldest chip
you will ship. For development boards or samples, confirm the actual chip
revision first with serial logs from `examples/esp-idf/00_board_check` or
ESP-IDF boot output.

## Maintaining Example Defaults

Do not add `CONFIG_ESP32P4_SELECTS_REV_LESS_V3` or `CONFIG_ESP32P4_REV_MIN_*`
directly to example `sdkconfig.defaults` files unless an example truly requires
one chip family. Prefer the shared overlays in `config/` so beginners and
advanced users can switch hardware revisions without editing every example.
