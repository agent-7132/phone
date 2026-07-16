# ESP32-P4 版本配置

[English Version](./ESP32P4_REVISION_CONFIG.md)

在 ESP-IDF 中，早于 rev v3.0 的 ESP32-P4 芯片与 rev v3.0 及之后的芯片并不二进制兼容。固件镜像必须针对这个边界的其中一侧进行构建。

本仓库中的示例不会在基础 `sdkconfig.defaults` 文件中硬编码早期工程样片版本。当你需要把构建固定到某个具体芯片版本系列时，请使用下面的仓库 overlay。

## 应该使用哪个配置？

| 开发板上的芯片 | 推荐 overlay | 说明 |
| --- | --- | --- |
| ESP32-P4 rev v3.1 或更新版本 | `../../../config/esp32p4_rev_v3_1.defaults` | 匹配 ESP-IDF v5.5 默认的最小芯片版本。 |
| ESP32-P4 rev v3.0 或更新版本 | `../../../config/esp32p4_rev_v3_0.defaults` | 当量产硬件包含 v3.0 芯片时使用。 |
| ESP32-P4 rev v0.x 或 v1.x 工程样片 | `../../../config/esp32p4_rev_pre_v3.defaults` | 仅支持 pre-v3 芯片，不支持 v3.x 芯片。 |

如果没有传入版本 overlay，ESP-IDF 会选择自己的默认值。在 ESP-IDF v5.5 中，普通构建的默认值是 rev v3.1 或更新版本。

## 构建命令

请在选定的 ESP-IDF 示例目录中运行命令。

为 rev v3.1 或更新的量产芯片构建：

```bash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;../../../config/esp32p4_rev_v3_1.defaults" set-target esp32p4 build
```

为 rev v3.0 或更新的量产芯片构建：

```bash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;../../../config/esp32p4_rev_v3_0.defaults" set-target esp32p4 build
```

为更早的工程样片构建：

```bash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;../../../config/esp32p4_rev_pre_v3.defaults" set-target esp32p4 build
```

当列出基础 `sdkconfig.defaults` 文件时，ESP-IDF 会自动追加目标相关的默认配置，例如 `sdkconfig.defaults.esp32p4`。这可以让 `17_simple_video_server` 等示例继续使用相同形态的命令。

当把已有示例目录从一个芯片版本系列切换到另一个版本系列时，请先删除本地生成的 `sdkconfig`，或为每个配置使用单独的 `SDKCONFIG` 文件。已有的 `sdkconfig` 选项优先级高于 defaults，否则可能隐藏你原本想测试的 overlay。

## 为什么这很重要

`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` 会选择早于 v3.0 的 ESP32-P4 版本，并排除 v3.x 芯片。在 ESP-IDF 中，这个选择会影响低层 SoC 支持，例如 eFuse 布局、PMU 和电源设置、睡眠支持、PSRAM、flash 时序，以及其他依赖硬件的代码路径。

对于量产，请选择与你计划出货的最老芯片匹配的最小版本。对于开发板或样片，请先通过 `examples/esp-idf/00_board_check` 的串口日志或 ESP-IDF 启动输出确认实际芯片版本。

## 维护示例默认配置

除非某个示例确实只适用于一个芯片版本系列，否则不要把 `CONFIG_ESP32P4_SELECTS_REV_LESS_V3` 或 `CONFIG_ESP32P4_REV_MIN_*` 直接添加到示例 `sdkconfig.defaults` 文件中。优先使用 `config/` 中共享的 overlay，这样初学者和高级用户都可以在不编辑每个示例的情况下切换硬件版本。
