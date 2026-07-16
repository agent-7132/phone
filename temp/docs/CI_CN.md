# 持续集成

[English Version](./CI.md)

本仓库使用 GitHub Actions 在 ESP-IDF 示例新增或变更时构建这些示例。

## ESP-IDF 示例构建

`ESP-IDF examples` 工作流会在以下情况下运行：

- Pull request 修改了 `examples/esp-idf/` 下的文件。
- Pull request 修改了 `config/` 下共享的 ESP32-P4 芯片版本默认配置。
- 推送到 `main` 且触及相同路径。
- 从 GitHub Actions 页面手动运行。

该工作流会查找 `examples/esp-idf/` 下同时包含以下内容的目录，以发现可构建示例：

- `CMakeLists.txt`
- `main/`

对于 pull request 和 push，只会构建发生变化的示例。如果共享配置文件或工作流本身发生变化，则会构建所有 ESP-IDF 示例。

手动运行时接受一个输入：

| 输入 | 值 |
| --- | --- |
| `example` | `all`、目录名（例如 `00_board_check`），或完整路径（例如 `examples/esp-idf/19_system_monitor`） |

当前工作流使用以下构建环境：

- ESP-IDF Docker image `espressif/idf:v5.5.4`：构建所有发现的示例。
- ESP-IDF Docker image `espressif/idf:v6.0.1`：构建所有发现的示例。
- Target：`esp32p4`

Arduino 示例目前有意暂不由该工作流构建。
