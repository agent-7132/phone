# ESP32-P4 Phone App开发指南

## 概述

本指南介绍如何为ESP32-P4 Phone系统开发应用程序。系统支持两种类型的应用：

| 类型 | 说明 | 适用场景 |
|------|------|----------|
| Native App | 编译时集成到固件中，性能最佳 | 需要高性能、复杂逻辑的核心应用 |
| Dynamic App | 运行时从SPIFFS加载，无需重新编译 | 快速开发、可热更新的轻量应用 |

## 系统架构

```
┌─────────────────────────────────────────────────────┐
│                    UI Layer                        │
│   Home Screen | Status Bar | App Screens           │
├─────────────────────────────────────────────────────┤
│                  App Manager                       │
│   Native Apps | Dynamic Apps | Lifecycle Mgmt      │
├─────────────────────────────────────────────────────┤
│                 App Installer                      │
│   Package Parsing | Installation | Registration    │
├─────────────────────────────────────────────────────┤
│               Dynamic App Engine                   │
│   JSON Layout Parser | Widget Builder | Commands   │
├─────────────────────────────────────────────────────┤
│                      BSP                           │
│   Display | Touch | I2C | SD Card | Audio         │
└─────────────────────────────────────────────────────┘
```

## 开发环境要求

- ESP-IDF v6.0.2
- Python 3.12+
- CMake 3.30+
- Ninja 1.12+
- RISC-V Toolchain (esp-15.2.0)

## 一、Native App开发

### 1.1 创建新的Native App

创建以下文件结构：

```
main/apps/
├── my_app.c
└── my_app.h
```

### 1.2 头文件模板 (my_app.h)

```c
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *my_app_create(void);

#ifdef __cplusplus
}
#endif
```

### 1.3 源文件模板 (my_app.c)

```c
#include "my_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "esp_log.h"

static const char *TAG = "MY_APP";

static void back_button_cb(lv_event_t *e)
{
    app_manager_go_home();
}

lv_obj_t *my_app_create(void)
{
    // 创建屏幕
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    // 添加状态栏
    status_bar_create(scr);

    // 添加标题
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "My App");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    // 添加返回按钮
    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_event_cb(back_btn, back_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "My app created");
    return scr;
}
```

### 1.4 注册Native App

在 `main.c` 的 `register_native_apps()` 函数中添加：

```c
memset(&app, 0, sizeof(app));
strcpy(app.name, "my_app");
strcpy(app.title, "My App");
strcpy(app.icon, "⭐");
app.type = APP_TYPE_NATIVE;
app.data.native.create_screen = my_app_create;
app_manager_register(&app);
```

### 1.5 更新CMakeLists.txt

在 `main/CMakeLists.txt` 中添加源文件：

```cmake
idf_component_register(SRCS "main.c"
                    "apps/my_app.c"
                    ...
                    INCLUDE_DIRS "." "system" "apps"
                    REQUIRES bsp)
```

## 二、Dynamic App开发

### 2.1 App包格式规范

每个Dynamic App是一个文件夹，位于 `/spiffs/apps/` 目录下：

```
/spiffs/apps/
└── my_dynamic_app/
    ├── manifest.json      # 必需：应用元数据
    ├── layout.json        # 必需：UI布局定义
    ├── icon.txt           # 可选：应用图标（emoji）
    └── resources/         # 可选：资源文件目录
```

### 2.2 manifest.json 格式

```json
{
    "name": "my_dynamic_app",
    "title": "My Dynamic App",
    "version": "1.0.0",
    "author": "Developer",
    "description": "A sample dynamic application",
    "icon": "📱",
    "entry_point": "layout.json",
    "type": "dynamic",
    "permissions": ["display", "touch"]
}
```

**字段说明：**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| name | string | 是 | 应用唯一标识符（小写字母+下划线） |
| title | string | 是 | 显示名称 |
| version | string | 否 | 版本号 |
| author | string | 否 | 作者 |
| description | string | 否 | 应用描述 |
| icon | string | 否 | Emoji图标，默认"📱" |
| entry_point | string | 否 | 入口文件，默认"layout.json" |
| type | string | 是 | "dynamic" |
| permissions | array | 否 | 权限列表 |

### 2.3 layout.json 格式

```json
{
    "screen": {
        "background_color": "#1a1a2e",
        "children": [
            {
                "type": "label",
                "id": "title",
                "text": "Hello World",
                "font": "montserrat_14",
                "color": "#FFFFFF",
                "align": "top_mid",
                "x": 0,
                "y": 50
            },
            {
                "type": "button",
                "id": "btn_click",
                "text": "Click Me",
                "width": 200,
                "height": 50,
                "bg_color": "#4a4a6a",
                "color": "#FFFFFF",
                "align": "center",
                "x": 0,
                "y": 0,
                "on_click": "show_message"
            },
            {
                "type": "rect",
                "id": "color_box",
                "width": 100,
                "height": 100,
                "bg_color": "#FF0000",
                "align": "center",
                "x": 0,
                "y": 100
            }
        ]
    },
    "commands": {
        "show_message": {
            "action": "set_text",
            "target": "title",
            "value": "Button Clicked!"
        },
        "go_home": {
            "action": "go_home"
        }
    }
}
```

### 2.4 支持的控件类型

#### Label（标签）

```json
{
    "type": "label",
    "id": "my_label",
    "text": "Hello",
    "font": "montserrat_14",
    "color": "#FFFFFF",
    "align": "center",
    "x": 0,
    "y": 0
}
```

#### Button（按钮）

```json
{
    "type": "button",
    "id": "my_button",
    "text": "Click",
    "width": 200,
    "height": 50,
    "bg_color": "#4a4a6a",
    "color": "#FFFFFF",
    "align": "center",
    "x": 0,
    "y": 0,
    "on_click": "command_name"
}
```

#### Rect（矩形）

```json
{
    "type": "rect",
    "id": "my_rect",
    "width": 100,
    "height": 100,
    "bg_color": "#FF0000",
    "align": "center",
    "x": 0,
    "y": 0
}
```

### 2.5 支持的对齐方式

- `top_left`, `top_mid`, `top_right`
- `left_mid`, `center`, `right_mid`
- `bottom_left`, `bottom_mid`, `bottom_right`

### 2.6 支持的命令动作

| 动作 | 参数 | 说明 |
|------|------|------|
| set_text | target, value | 设置指定控件的文本 |
| go_home | 无 | 返回主屏幕 |

### 2.7 安装Dynamic App

将应用文件夹复制到SPIFFS的 `/apps/` 目录下：

```bash
# 方法1：使用idf.py partition-table-flash和spiffs-flash
python -m spiffsgen 8388608 apps_spiffs spiffs.bin
python -m esptool --chip esp32p4 write_flash 0x110000 spiffs.bin

# 方法2：通过SD卡安装
# 将app文件夹复制到SD卡的/apps/目录下
# 在SD卡应用中使用安装功能
```

## 三、BSP API参考

### 3.1 显示API

```c
#include "bsp/display.h"

// 初始化显示
lv_display_t *disp = bsp_display_start_with_config(&cfg);

// 背光控制
bsp_display_backlight_on();
bsp_display_backlight_off();
bsp_display_set_backlight(brightness);

// 显示锁定（用于多任务）
bsp_display_lock(portMAX_DELAY);
bsp_display_unlock();
```

### 3.2 触摸API

```c
#include "bsp/touch.h"

// 获取触摸点
lv_indev_t *indev = bsp_touch_init();
lv_point_t point;
lv_indev_get_point(indev, &point);
```

### 3.3 I2C API

```c
#include "bsp/esp32_p4_platform.h"

// 初始化I2C
bsp_i2c_init();

// 获取I2C总线句柄
i2c_master_bus_handle_t bus = bsp_i2c_get_bus();
```

### 3.4 SD卡API

```c
#include "bsp/sdcard.h"

// 挂载SD卡
esp_err_t ret = bsp_sdcard_mount();

// 卸载SD卡
bsp_sdcard_unmount();
```

### 3.5 SPIFFS API

```c
#include "bsp/spiffs.h"

// 挂载SPIFFS
esp_err_t ret = bsp_spiffs_mount();

// 卸载SPIFFS
bsp_spiffs_unmount();
```

### 3.6 音频API

```c
#include "bsp/audio.h"

// 初始化音频
esp_codec_dev_handle_t speaker = bsp_audio_init();

// 设置音量
esp_codec_dev_set_out_vol(speaker, volume);
```

## 四、LVGL常用控件示例

### 4.1 创建标签

```c
lv_obj_t *label = lv_label_create(parent);
lv_label_set_text(label, "Hello World");
lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
```

### 4.2 创建按钮

```c
lv_obj_t *btn = lv_btn_create(parent);
lv_obj_set_size(btn, 200, 50);
lv_obj_set_style_bg_color(btn, lv_color_hex(0x4a4a6a), 0);
lv_obj_set_style_bg_color(btn, lv_color_hex(0x5a5a8a), LV_STATE_PRESSED);
lv_obj_add_event_cb(btn, my_callback, LV_EVENT_CLICKED, NULL);

lv_obj_t *btn_label = lv_label_create(btn);
lv_label_set_text(btn_label, "Click Me");
lv_obj_center(btn_label);
```

### 4.3 创建滑块

```c
lv_obj_t *slider = lv_slider_create(parent);
lv_obj_set_size(slider, 300, 25);
lv_slider_set_range(slider, 0, 100);
lv_slider_set_value(slider, 50, LV_ANIM_OFF);
lv_obj_add_event_cb(slider, value_changed, LV_EVENT_VALUE_CHANGED, NULL);
```

### 4.4 创建容器

```c
lv_obj_t *container = lv_obj_create(parent);
lv_obj_set_size(container, 400, 300);
lv_obj_set_style_bg_color(container, lv_color_hex(0x252540), 0);
lv_obj_set_style_border_width(container, 1, 0);
lv_obj_set_style_border_color(container, lv_color_hex(0x3a3a5a), 0);
```

## 五、调试方法

### 5.1 日志输出

```c
#include "esp_log.h"

static const char *TAG = "MY_APP";

ESP_LOGI(TAG, "Info message");
ESP_LOGW(TAG, "Warning message");
ESP_LOGE(TAG, "Error message");
ESP_LOGD(TAG, "Debug message");
```

### 5.2 串口输出

确保设备通过USB连接，使用以下命令查看日志：

```bash
idf.py monitor
```

### 5.3 LVGL调试

启用LVGL日志：

```c
#define LV_LOG_LEVEL LV_LOG_LEVEL_INFO
```

## 六、构建与烧录

### 6.1 构建项目

```bash
cd e:/phone
idf.py build
```

### 6.2 烧录到设备

```bash
idf.py -p COM3 flash
```

### 6.3 构建并烧录

```bash
idf.py -p COM3 build flash monitor
```

## 七、项目目录结构

```
esp32-p4-phone/
├── main/                      # 主应用代码
│   ├── apps/                  # Native应用
│   │   ├── display_app.c/h
│   │   ├── touch_app.c/h
│   │   ├── i2c_app.c/h
│   │   ├── sdcard_app.c/h
│   │   ├── audio_app.c/h
│   │   ├── settings_app.c/h
│   │   └── info_app.c/h
│   ├── system/                # 系统模块
│   │   ├── app_manager.c/h
│   │   ├── app_installer.c/h
│   │   ├── dynamic_app_engine.c/h
│   │   ├── home_screen.c/h
│   │   └── status_bar.c/h
│   ├── CMakeLists.txt
│   └── main.c
├── components/                # 组件
│   ├── bsp/                   # BSP板级支持包
│   └── bsp_st7102/            # ST7102显示驱动
├── managed_components/        # 管理组件
├── partitions.csv             # 分区表配置
├── sdkconfig                  # ESP-IDF配置
└── CMakeLists.txt             # 项目构建配置
```

## 八、性能优化建议

1. **内存管理**：使用 `heap_caps_malloc(MALLOC_CAP_SPIRAM)` 分配大内存到PSRAM
2. **LVGL优化**：启用LVGL的DMA2D支持和双缓冲
3. **任务优先级**：合理设置FreeRTOS任务优先级
4. **资源压缩**：压缩图片资源，使用合适的字体大小

## 九、常见问题

### Q: Dynamic App无法加载？
A: 检查SPIFFS是否正确挂载，确认manifest.json格式正确，检查路径是否正确。

### Q: 触摸坐标不正确？
A: 在BSP配置中调整`swap_xy`、`mirror_x`、`mirror_y`参数。

### Q: 编译错误"undefined reference"?
A: 检查CMakeLists.txt是否包含了源文件，检查头文件是否正确包含。

### Q: 内存不足？
A: 减少LVGL对象数量，使用PSRAM，优化图片资源。
