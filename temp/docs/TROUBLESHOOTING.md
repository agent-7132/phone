# Troubleshooting

[中文版本](./TROUBLESHOOTING_CN.md)

Start with `examples/esp-idf/00_board_check`. It has no external hardware
requirements and helps separate toolchain problems from peripheral problems.

## Build Problems

### `idf.py` is not found

Open an ESP-IDF terminal or source the ESP-IDF export script before building.
Then run:

```bash
idf.py --version
```

### Wrong target

Most examples in this repository target ESP32-P4. From the example directory,
run:

```bash
idf.py set-target esp32p4
```

### ESP32-P4 revision mismatch

ESP32-P4 chips earlier than rev v3.0 and chips at rev v3.0 or later are
mutually exclusive build targets. If an image builds but does not boot on a
different P4 revision family, rebuild with the correct overlay from
[ESP32-P4 Revision Config](ESP32P4_REVISION_CONFIG.md).

For early engineering samples:

```bash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;../../../config/esp32p4_rev_pre_v3.defaults" set-target esp32p4 build
```

For production chips, prefer the documented v3.0 or v3.1 overlay, or let
ESP-IDF use its default production revision profile.

### Managed component download fails

Some examples fetch dependencies through `idf_component.yml`. Check your
network connection and retry the build. If you are behind a proxy, configure
ESP-IDF and Git proxy settings first.

### VS Code plugin build fails

Some networking examples include a legacy note that they may fail when built
through a VS Code plugin workflow. Use an ESP-IDF terminal first:

```bash
cd examples/esp-idf/10_wifistation
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
```

After the terminal build succeeds, compare the VS Code plugin environment,
target, and configuration.

## Flash and Monitor Problems

### Serial port is missing

- Use a data-capable USB cable.
- Check Device Manager on Windows or `/dev/tty*` on Linux/macOS.
- Close other serial monitor programs.
- Press the board reset or boot button if your board requires manual boot mode.

### Flash succeeds but monitor is unreadable

Check the monitor baud rate. ESP-IDF defaults are usually correct when using:

```bash
idf.py -p PORT flash monitor
```

## Runtime Problems

### PSRAM is not initialized

Many display, LVGL, camera, and Arduino examples require PSRAM. Confirm PSRAM
with `00_board_check`, then check the example's `sdkconfig.defaults` and board
settings.

### Wi-Fi does not connect

- Confirm the board has Wi-Fi support or a configured Wi-Fi companion path.
- Run `idf.py menuconfig` and set SSID/password.
- Check whether the access point is 2.4 GHz and reachable.
- Watch the serial log for authentication, timeout, or retry messages.

### Ethernet link is down

- Confirm the board has Ethernet hardware.
- Check PHY model, PHY address, MDC/MDIO GPIOs, reset GPIO, and clock mode.
- Confirm cable, switch, and DHCP server.

### SD card mount fails

- Confirm the card is inserted and formatted.
- Try 1-line SDMMC mode if wiring or signal integrity is uncertain.
- Do not enable formatting options unless you are willing to erase the card.

### Display is blank

- Confirm the selected panel and `CURRENT_SCREEN` or BSP configuration.
- Confirm PSRAM is enabled.
- Check backlight control first, then panel init logs.
- Start with a simple color bar or Arduino `HelloWorld` before LVGL demos.

### Touch does not respond

- Confirm the touch controller model and I2C pins.
- Use an I2C scan to check whether the touch controller appears on the bus.
- Verify reset and interrupt pins when the touch driver requires them.

## Asking for Help

When opening an issue, include:

- Board name and revision.
- Example path.
- ESP-IDF or Arduino-ESP32 version.
- Full build command.
- Full error log or serial output.
- Photos or wiring notes when external hardware is involved.
