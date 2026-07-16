# Getting Started

[中文版本](./GETTING_STARTED_CN.md)

This guide is written for first-time users. If you already know ESP-IDF or
Arduino, use [EXAMPLES_GUIDE.md](EXAMPLES_GUIDE.md) to jump directly to an
example.

## Choose Your Path

| You want to... | Start here |
| --- | --- |
| Confirm the board and toolchain work | `examples/esp-idf/00_board_check` |
| Learn the basic ESP-IDF build and monitor flow | `examples/esp-idf/02_HelloWorld` |
| Use a display from Arduino | `examples/arduino/examples/HelloWorld` |
| Bring up a hardware peripheral | Pick from [EXAMPLES_GUIDE.md](EXAMPLES_GUIDE.md) |
| Debug a build, flash, or boot problem | [TROUBLESHOOTING.md](TROUBLESHOOTING.md) |

## ESP-IDF First Run

1. Install ESP-IDF. The examples are mainly prepared for ESP-IDF release/v5.4
   or later unless an example README says otherwise.
2. Open an ESP-IDF terminal.
3. Clone this repository.
4. Build the board check example:

```bash
cd examples/esp-idf/00_board_check
idf.py set-target esp32p4
idf.py build
```

5. If your board uses an early ESP32-P4 engineering sample earlier than rev
   v3.0, build with the pre-v3 overlay documented in
   [ESP32-P4 Revision Config](ESP32P4_REVISION_CONFIG.md). Production v3.x
   boards can use the normal build flow, or explicitly select the v3.0/v3.1
   overlay when you want a locked production profile.
6. Flash and open the monitor:

```bash
idf.py -p PORT flash monitor
```

Replace `PORT` with your serial port.

If you see the board check banner and periodic `alive` logs, your environment
is ready.

## Arduino First Run

1. Install Arduino IDE or arduino-cli.
2. Install a compatible Arduino-ESP32 core. See
   [examples/arduino/README.md](../examples/arduino/README.md).
3. Enable PSRAM in the Arduino board settings.
4. Open `examples/arduino/examples/HelloWorld/HelloWorld.ino`.
5. Select the matching board and upload port.
6. Upload the sketch.

Most Arduino examples in this repository are display-oriented. They require a
compatible DSI display configuration and PSRAM.

## Beginner Checklist

- Use a data-capable USB cable, not a charge-only cable.
- Confirm the serial port appears when the board is connected.
- Start with `00_board_check` before examples that need Wi-Fi, Ethernet, SD
  card, display, touch, audio, or camera hardware.
- Confirm whether your ESP32-P4 chip is a production v3.x chip or an earlier
  engineering sample. The build profile must match that revision family.
- Read the README inside the selected example directory.
- If an example has `menuconfig` settings, configure them before building.

## What to Read Next

- [EXAMPLES_GUIDE.md](EXAMPLES_GUIDE.md): choose an example by difficulty and
  peripheral.
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md): common setup and runtime problems.
- [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md): repository layout for
  contributors.
