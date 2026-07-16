# 入门指南

[English Version](./GETTING_STARTED.md)

本指南面向首次使用的用户。如果你已经熟悉 ESP-IDF 或 Arduino，可以使用 [示例指南](EXAMPLES_GUIDE_CN.md) 直接跳到合适示例。

## 选择路径

| 你想要... | 从这里开始 |
| --- | --- |
| 确认开发板和工具链可以工作 | `examples/esp-idf/00_board_check` |
| 学习基础 ESP-IDF build 和 monitor 流程 | `examples/esp-idf/02_HelloWorld` |
| 从 Arduino 使用显示屏 | `examples/arduino/examples/HelloWorld` |
| bring up 一个硬件外设 | 从 [示例指南](EXAMPLES_GUIDE_CN.md) 选择 |
| 调试 build、flash 或 boot 问题 | [故障排查](TROUBLESHOOTING_CN.md) |

## 第一次运行 ESP-IDF

1. 安装 ESP-IDF。除非某个示例 README 另有说明，这些示例主要面向 ESP-IDF release/v5.4 或更新版本。
2. 打开 ESP-IDF terminal。
3. 克隆本仓库。
4. 构建 board check 示例：

```bash
cd examples/esp-idf/00_board_check
idf.py set-target esp32p4
idf.py build
```

5. 如果你的开发板使用早于 rev v3.0 的早期 ESP32-P4 工程样片，请使用 [ESP32-P4 版本配置](ESP32P4_REVISION_CONFIG_CN.md) 中记录的 pre-v3 overlay 构建。量产 v3.x 开发板可以使用常规构建流程；当你需要锁定量产 profile 时，也可以显式选择 v3.0/v3.1 overlay。
6. 烧录并打开 monitor：

```bash
idf.py -p PORT flash monitor
```

将 `PORT` 替换为你的串口。

如果看到 board check banner 和周期性的 `alive` 日志，说明你的环境已经准备好。

## 第一次运行 Arduino

1. 安装 Arduino IDE 或 arduino-cli。
2. 安装兼容的 Arduino-ESP32 core。参见 [examples/arduino/README.md](../examples/arduino/README.md)。
3. 在 Arduino board settings 中启用 PSRAM。
4. 打开 `examples/arduino/examples/HelloWorld/HelloWorld.ino`。
5. 选择匹配的开发板和 upload port。
6. 上传 sketch。

本仓库中的大多数 Arduino 示例都偏向显示应用。它们需要兼容的 DSI display 配置和 PSRAM。

## 初学者检查清单

- 使用支持数据传输的 USB 线，而不是仅充电线。
- 确认开发板连接后串口会出现。
- 在运行需要 Wi-Fi、Ethernet、SD card、display、touch、audio 或 camera 硬件的示例之前，先从 `00_board_check` 开始。
- 确认你的 ESP32-P4 芯片是量产 v3.x 芯片，还是更早的工程样片。构建 profile 必须匹配该版本系列。
- 阅读所选示例目录中的 README。
- 如果示例有 `menuconfig` 设置，请在构建前完成配置。

## 接下来阅读什么

- [示例指南](EXAMPLES_GUIDE_CN.md)：按难度和外设选择示例。
- [故障排查](TROUBLESHOOTING_CN.md)：常见设置和运行时问题。
- [项目结构](PROJECT_STRUCTURE_CN.md)：面向贡献者的仓库布局。
