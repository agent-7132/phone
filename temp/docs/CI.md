# Continuous Integration

[中文版本](./CI_CN.md)

This repository uses GitHub Actions to build ESP-IDF examples when they are
added or changed.

## ESP-IDF Example Builds

The `ESP-IDF examples` workflow runs on:

- Pull requests that change files under `examples/esp-idf/`.
- Pull requests that change shared ESP32-P4 revision defaults under `config/`.
- Pushes to `main` that touch the same paths.
- Manual runs from the GitHub Actions page.

The workflow discovers buildable examples by looking for directories under
`examples/esp-idf/` that contain both:

- `CMakeLists.txt`
- `main/`

For pull requests and pushes, only changed examples are built. If a shared
configuration file or the workflow itself changes, the workflow builds all
ESP-IDF examples.

Manual runs accept one input:

| Input | Value |
| --- | --- |
| `example` | `all`, a directory name such as `00_board_check`, or a full path such as `examples/esp-idf/19_system_monitor` |

The workflow currently builds with:

- ESP-IDF Docker image `espressif/idf:v5.5.4` for all discovered examples.
- ESP-IDF Docker image `espressif/idf:v6.0.1` for all discovered examples.
- Target: `esp32p4`

Arduino examples are intentionally not built by this workflow yet.
