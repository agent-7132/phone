# ESP32-P4 Board Support Package (BSP)

## 概述

本BSP是为Waveshare ESP32-P4平台设计的板级支持包，提供以下硬件组件的驱动支持：

- **LCD显示**：支持多种分辨率的MIPI DSI接口LCD（3.4"~10.1"）
- **触摸屏**：ST7123电容触摸屏驱动
- **音频**：ES8311编解码器，支持扬声器和麦克风
- **I2C总线**：主设备驱动，支持多设备挂载
- **SD卡**：SDMMC接口，支持高速模式
- **SPIFFS**：SPI Flash文件系统
- **USB主机**：USB Host模式支持

## 硬件平台

- **芯片**：ESP32-P4
- **I2C引脚**：SCL=GPIO8, SDA=GPIO7
- **I2S引脚**：SCLK=GPIO12, MCLK=GPIO13, LCLK=GPIO10, DOUT=GPIO9, DSIN=GPIO11
- **SD卡引脚**：D0=GPIO39, D1=GPIO40, D2=GPIO41, D3=GPIO42, CMD=GPIO44, CLK=GPIO43
- **音频功放**：GPIO53

## 目录结构

```
components/bsp/
├── include/
│   └── bsp/
│       ├── config.h          # BSP配置宏定义
│       ├── display.h         # 显示驱动接口
│       ├── esp-bsp.h         # BSP总入口头文件
│       ├── esp32_p4_platform.h # 平台接口声明
│       └── touch.h           # 触摸屏驱动接口
├── priv_include/
│   └── bsp_err_check.h       # 错误检查宏
├── CMakeLists.txt            # CMake构建配置
├── Kconfig                   # 菜单配置选项
├── LICENSE                   # 许可证
├── README.md                 # 本文档
├── esp32_p4_platform.c       # 平台驱动实现
└── idf_component.yml         # 组件依赖配置
```

## ESP-IDF版本兼容性

| ESP-IDF版本 | 状态 | 说明 |
|------------|------|------|
| v5.5.x | 支持 | 完整功能支持 |
| v6.0.x | 支持 | 完整功能支持，部分API调整 |
| v6.x.x | 支持 | 持续兼容 |

## 编译配置选项

### I2C配置

```
BSP_I2C_NUM              - I2C外设编号（默认：1）
BSP_I2C_FAST_MODE        - 启用快速模式（默认：开启，400kHz）
BSP_I2C_CLK_SPEED_HZ     - I2C时钟频率（默认：400000）
```

### I2S配置

```
BSP_I2S_NUM              - I2S外设编号（默认：1）
```

### SD卡配置

```
BSP_SD_FORMAT_ON_MOUNT_FAIL - 挂载失败时格式化（默认：关闭）
BSP_SD_MOUNT_POINT      - SD卡挂载点（默认：/sdcard）
```

### SPIFFS配置

```
BSP_SPIFFS_FORMAT_ON_MOUNT_FAIL - 挂载失败时格式化（默认：关闭）
BSP_SPIFFS_MOUNT_POINT  - SPIFFS挂载点（默认：/spiffs）
BSP_SPIFFS_PARTITION_LABEL - SPIFFS分区标签（默认：storage）
BSP_SPIFFS_MAX_FILES    - 最大支持文件数（默认：5）
```

### 显示配置

```
BSP_LCD_DPI_BUFFER_NUMS  - DPI帧缓冲数量（1-3，默认：3）
BSP_DISPLAY_BRIGHTNESS_LEDC_CH - LEDC通道（默认：1）
BSP_LCD_COLOR_FORMAT    - 颜色格式（RGB565/RGB888，默认：RGB565）
BSP_LCD_TYPE            - LCD类型选择（见下表）
```

### LCD类型列表

| 配置选项 | 分辨率 | 尺寸 | 控制器 |
|---------|--------|------|--------|
| BSP_LCD_TYPE_800_800_3_4_INCH | 800x800 | 3.4" | JD9365 |
| BSP_LCD_TYPE_720_720_4_INCH | 720x720 | 4" | JD9365 |
| BSP_LCD_TYPE_480_800_4_INCH_A | 480x800 | 4" | ST7701 |
| BSP_LCD_TYPE_480_800_4_3_INCH_A | 480x800 | 4.3" | ST7701 |
| BSP_LCD_TYPE_720_1280_5_INCH_A | 720x1280 | 5" | HX8394 |
| BSP_LCD_TYPE_720_1280_7_INCH_A | 720x1280 | 7" | ILI9881C |
| BSP_LCD_TYPE_1024_600_7_INCH_C | 1024x600 | 7" | EK79007 |
| BSP_LCD_TYPE_800_1280_8_INCH_A | 800x1280 | 8" | JD9365 |
| BSP_LCD_TYPE_480_1920_8_8_INCH_A | 480x1920 | 8.8" | OTA7290B |
| BSP_LCD_TYPE_720_1280_9_INCH_B | 720x1280 | 9" | JD9365 |
| BSP_LCD_TYPE_800_1280_10_1_INCH_A | 800x1280 | 10.1" | JD9365 |
| BSP_LCD_TYPE_720_1280_10_1_INCH_B | 720x1280 | 10.1" | JD9365 |

## API参考

### 1. I2C接口

#### bsp_i2c_init

```c
esp_err_t bsp_i2c_init(void);
```

**功能**：初始化I2C主设备驱动

**返回值**：
- ESP_OK：初始化成功
- ESP_ERR_INVALID_ARG：参数错误
- ESP_FAIL：驱动安装失败

**示例**：
```c
ESP_ERROR_CHECK(bsp_i2c_init());
```

#### bsp_i2c_deinit

```c
esp_err_t bsp_i2c_deinit(void);
```

**功能**：反初始化I2C驱动并释放资源

**返回值**：
- ESP_OK：成功
- ESP_ERR_INVALID_ARG：参数错误

#### bsp_i2c_get_handle

```c
i2c_master_bus_handle_t bsp_i2c_get_handle(void);
```

**功能**：获取I2C总线句柄

**返回值**：I2C主设备句柄

**示例**：
```c
i2c_master_bus_handle_t i2c_handle = bsp_i2c_get_handle();
```

---

### 2. I2S音频接口

#### bsp_audio_init

```c
esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config);
```

**功能**：初始化I2S音频接口

**参数**：
- i2s_config：I2S配置结构体指针，传NULL使用默认配置（单声道、全双工、16位、22050Hz）

**返回值**：
- ESP_OK：成功
- ESP_ERR_NOT_SUPPORTED：通信模式不支持
- ESP_ERR_INVALID_ARG：参数无效
- ESP_ERR_NOT_FOUND：无可用I2S通道
- ESP_ERR_NO_MEM：内存不足
- ESP_ERR_INVALID_STATE：通道未初始化或已启动

**示例**：
```c
ESP_ERROR_CHECK(bsp_audio_init(NULL));
```

#### bsp_audio_codec_speaker_init

```c
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);
```

**功能**：初始化扬声器编解码器设备

**返回值**：编解码器设备句柄，失败返回NULL

**示例**：
```c
esp_codec_dev_handle_t speaker = bsp_audio_codec_speaker_init();
if (speaker) {
    esp_codec_dev_set_out_vol(speaker, 50);
}
```

#### bsp_audio_codec_microphone_init

```c
esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void);
```

**功能**：初始化麦克风编解码器设备

**返回值**：编解码器设备句柄，失败返回NULL

**示例**：
```c
esp_codec_dev_handle_t mic = bsp_audio_codec_microphone_init();
if (mic) {
    esp_codec_dev_open(mic, &fs);
}
```

---

### 3. SPIFFS文件系统

#### bsp_spiffs_mount

```c
esp_err_t bsp_spiffs_mount(void);
```

**功能**：挂载SPIFFS文件系统到虚拟文件系统

**返回值**：
- ESP_OK：成功
- ESP_ERR_INVALID_STATE：已挂载
- ESP_ERR_NO_MEM：内存不足
- ESP_FAIL：挂载失败

**示例**：
```c
ESP_ERROR_CHECK(bsp_spiffs_mount());
FILE* f = fopen("/spiffs/data.txt", "r");
```

#### bsp_spiffs_unmount

```c
esp_err_t bsp_spiffs_unmount(void);
```

**功能**：卸载SPIFFS文件系统

**返回值**：
- ESP_OK：成功
- ESP_ERR_NOT_FOUND：分区不存在
- ESP_ERR_INVALID_STATE：已卸载

---

### 4. SD卡接口

#### bsp_sdcard_mount

```c
esp_err_t bsp_sdcard_mount(void);
```

**功能**：挂载SD卡到虚拟文件系统

**返回值**：
- ESP_OK：成功
- ESP_ERR_INVALID_STATE：已挂载
- ESP_ERR_NO_MEM：内存不足
- ESP_FAIL：挂载失败

**示例**：
```c
ESP_ERROR_CHECK(bsp_sdcard_mount());
FILE* f = fopen("/sdcard/video.mp4", "rb");
```

#### bsp_sdcard_unmount

```c
esp_err_t bsp_sdcard_unmount(void);
```

**功能**：卸载SD卡

**返回值**：
- ESP_OK：成功
- ESP_ERR_NOT_FOUND：分区不存在

**全局变量**：
```c
extern sdmmc_card_t *bsp_sdcard;  // SD卡设备句柄
```

---

### 5. LCD显示接口

#### bsp_display_new

```c
esp_err_t bsp_display_new(const bsp_display_config_t *config, 
                         esp_lcd_panel_handle_t *ret_panel, 
                         esp_lcd_panel_io_handle_t *ret_io);
```

**功能**：创建新的LCD显示面板

**参数**：
- config：显示配置（传NULL使用默认配置）
- ret_panel：输出参数，LCD面板句柄
- ret_io：输出参数，LCD IO句柄

**返回值**：
- ESP_OK：成功
- 其他：esp_lcd相关错误

**示例**：
```c
esp_lcd_panel_handle_t panel;
esp_lcd_panel_io_handle_t io;
ESP_ERROR_CHECK(bsp_display_new(NULL, &panel, &io));
esp_lcd_panel_disp_on_off(panel, true);
```

#### bsp_display_new_with_handles

```c
esp_err_t bsp_display_new_with_handles(const bsp_display_config_t *config, 
                                       bsp_lcd_handles_t *ret_handles);
```

**功能**：创建显示面板并返回所有句柄

**参数**：
- config：显示配置
- ret_handles：输出参数，包含所有LCD句柄

**返回值**：ESP_OK或错误码

**bsp_lcd_handles_t结构体**：
```c
typedef struct {
    esp_lcd_dsi_bus_handle_t    mipi_dsi_bus;  // MIPI DSI总线句柄
    esp_lcd_panel_io_handle_t   io;            // LCD IO句柄
    esp_lcd_panel_handle_t      panel;         // LCD面板句柄
    esp_lcd_panel_handle_t      control;       // 控制面板句柄（通常为NULL）
} bsp_lcd_handles_t;
```

#### bsp_display_brightness_init

```c
esp_err_t bsp_display_brightness_init(void);
```

**功能**：初始化显示亮度控制

**返回值**：ESP_OK或错误码

#### bsp_display_brightness_set

```c
esp_err_t bsp_display_brightness_set(int brightness_percent);
```

**功能**：设置显示亮度

**参数**：
- brightness_percent：亮度百分比（0-100）

**返回值**：ESP_OK或错误码

**示例**：
```c
ESP_ERROR_CHECK(bsp_display_brightness_set(50));  // 设置50%亮度
```

#### bsp_display_backlight_on

```c
esp_err_t bsp_display_backlight_on(void);
```

**功能**：打开显示背光（亮度设为100%）

**返回值**：ESP_OK或错误码

#### bsp_display_backlight_off

```c
esp_err_t bsp_display_backlight_off(void);
```

**功能**：关闭显示背光（亮度设为0%）

**返回值**：ESP_OK或错误码

---

### 6. 触摸屏接口

#### bsp_touch_new

```c
esp_err_t bsp_touch_new(const bsp_display_cfg_t *cfg, esp_lcd_touch_handle_t *ret_touch);
```

**功能**：创建新的触摸屏设备

**参数**：
- cfg：显示配置结构体（包含触摸标志）
- ret_touch：输出参数，触摸屏句柄

**返回值**：
- ESP_OK：成功
- ESP_ERR_NOT_FOUND：未检测到触摸屏
- 其他：esp_lcd_touch相关错误

**示例**：
```c
bsp_display_cfg_t cfg = {
    .touch_flags = {
        .swap_xy = 0,
        .mirror_x = 0,
        .mirror_y = 0,
    }
};
esp_lcd_touch_handle_t touch;
ESP_ERROR_CHECK(bsp_touch_new(&cfg, &touch));
```

---

### 7. LVGL图形库接口（需启用图形库）

#### bsp_display_start

```c
lv_display_t *bsp_display_start(void);
```

**功能**：初始化显示并启动LVGL（使用默认配置）

**返回值**：LVGL显示句柄，失败返回NULL

**示例**：
```c
lv_display_t *disp = bsp_display_start();
if (disp) {
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello World");
    lv_obj_center(label);
}
```

#### bsp_display_start_with_config

```c
lv_display_t *bsp_display_start_with_config(bsp_display_cfg_t *cfg);
```

**功能**：使用自定义配置初始化显示并启动LVGL

**参数**：
- cfg：显示配置结构体

**返回值**：LVGL显示句柄，失败返回NULL

**bsp_display_cfg_t结构体**：
```c
typedef struct {
    esp_lv_adapter_config_t          lv_adapter_cfg;     // LVGL适配器配置
    esp_lv_adapter_rotation_t        rotation;           // 屏幕旋转角度
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode;    // 防撕裂模式
    struct {
        unsigned int swap_xy;   // 交换X/Y坐标
        unsigned int mirror_x;  // X轴镜像
        unsigned int mirror_y;  // Y轴镜像
    } touch_flags;              // 触摸屏标志
} bsp_display_cfg_t;
```

#### bsp_display_get_input_dev

```c
lv_indev_t *bsp_display_get_input_dev(void);
```

**功能**：获取LVGL输入设备（触摸屏）句柄

**返回值**：LVGL输入设备句柄

#### bsp_display_lock

```c
esp_err_t bsp_display_lock(uint32_t timeout_ms);
```

**功能**：获取LVGL互斥锁（线程安全）

**参数**：
- timeout_ms：超时时间（毫秒）

**返回值**：ESP_OK或错误码

#### bsp_display_unlock

```c
void bsp_display_unlock(void);
```

**功能**：释放LVGL互斥锁

**示例（线程安全操作）**：
```c
bsp_display_lock(portMAX_DELAY);
// LVGL操作
lv_obj_t *btn = lv_btn_create(lv_scr_act());
bsp_display_unlock();
```

#### bsp_display_get_panel_handle

```c
esp_lcd_panel_handle_t bsp_display_get_panel_handle(void);
```

**功能**：获取LCD面板句柄

**返回值**：LCD面板句柄

---

### 8. USB主机接口

#### bsp_usb_host_start

```c
esp_err_t bsp_usb_host_start(bsp_usb_host_power_mode_t mode, bool limit_500mA);
```

**功能**：启动USB主机模式

**参数**：
- mode：USB主机电源模式（本平台仅支持BSP_USB_HOST_POWER_MODE_USB_DEV）
- limit_500mA：限制输出电流为500mA（本平台未使用）

**返回值**：
- ESP_OK：成功
- ESP_ERR_INVALID_ARG：参数错误
- ESP_ERR_NO_MEM：内存不足

**示例**：
```c
ESP_ERROR_CHECK(bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, false));
```

#### bsp_usb_host_stop

```c
esp_err_t bsp_usb_host_stop(void);
```

**功能**：停止USB主机模式

**返回值**：
- ESP_OK：成功
- ESP_ERR_INVALID_ARG：参数错误

---

## 完整初始化示例

### 基础初始化（无图形库）

```c
#include "bsp/esp-bsp.h"

void app_main(void)
{
    // 初始化I2C
    ESP_ERROR_CHECK(bsp_i2c_init());
    
    // 挂载SD卡
    ESP_ERROR_CHECK(bsp_sdcard_mount());
    
    // 挂载SPIFFS
    ESP_ERROR_CHECK(bsp_spiffs_mount());
    
    // 初始化音频
    ESP_ERROR_CHECK(bsp_audio_init(NULL));
    esp_codec_dev_handle_t speaker = bsp_audio_codec_speaker_init();
    
    // 初始化显示
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t io;
    ESP_ERROR_CHECK(bsp_display_new(NULL, &panel, &io));
    ESP_ERROR_CHECK(bsp_display_brightness_set(100));
    esp_lcd_panel_disp_on_off(panel, true);
    
    // 初始化触摸屏
    bsp_display_cfg_t cfg = {0};
    esp_lcd_touch_handle_t touch;
    ESP_ERROR_CHECK(bsp_touch_new(&cfg, &touch));
    
    // 启动USB主机
    ESP_ERROR_CHECK(bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, false));
}
```

### LVGL图形库初始化

```c
#include "bsp/esp-bsp.h"

void app_main(void)
{
    // 初始化显示和LVGL
    lv_display_t *disp = bsp_display_start();
    if (!disp) {
        ESP_LOGE(TAG, "Display init failed");
        return;
    }
    
    // 设置亮度
    bsp_display_brightness_set(80);
    
    // 创建UI
    bsp_display_lock(portMAX_DELAY);
    
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "ESP32-P4 PhoneUI");
    lv_obj_center(label);
    
    bsp_display_unlock();
    
    // 其他初始化
    ESP_ERROR_CHECK(bsp_sdcard_mount());
    ESP_ERROR_CHECK(bsp_spiffs_mount());
}
```

---

## 宏定义参考

### 板级能力定义

```c
#define BSP_CAPS_DISPLAY        1    // 支持显示
#define BSP_CAPS_TOUCH          1    // 支持触摸
#define BSP_CAPS_BUTTONS        0    // 不支持按键
#define BSP_CAPS_AUDIO          1    // 支持音频
#define BSP_CAPS_AUDIO_SPEAKER  1    // 支持扬声器
#define BSP_CAPS_AUDIO_MIC      1    // 支持麦克风
#define BSP_CAPS_SDCARD         1    // 支持SD卡
#define BSP_CAPS_IMU            0    // 不支持IMU
```

### LCD分辨率宏

根据配置的LCD类型，自动定义以下宏：

```c
BSP_LCD_H_RES           // 水平分辨率
BSP_LCD_V_RES           // 垂直分辨率
BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS  // MIPI DSI通道比特率
BSP_LCD_COLOR_FORMAT    // 颜色格式
BSP_LCD_COLOR_SPACE     // 颜色空间（RGB）
```

---

## 注意事项

1. **LVGL线程安全**：调用LVGL API前必须获取互斥锁（bsp_display_lock），调用完成后释放（bsp_display_unlock）

2. **内存管理**：
   - 显示相关句柄通过esp_lcd API释放
   - 音频资源通过i2s_del_channel释放
   - USB主机资源通过bsp_usb_host_stop释放

3. **电源管理**：
   - MIPI DSI PHY电源由BSP自动管理
   - SD卡电源通过片上LDO管理
   - 音频功放由ES8311编解码器控制

4. **触摸屏兼容性**：
   - 支持ST7123电容触摸屏（I2C接口）
   - 默认I2C地址：0x38

5. **LCD背光控制**：
   - 通过I2C接口控制背光亮度（芯片地址0x45）
   - 亮度范围：0-100%

---

## 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| 2.0.3 | 2026-07 | 触摸驱动从GT911更换为ST7123 |
| 2.0.2 | 2026-07 | 支持ESP-IDF v6.0，USB主机API调整 |
| 2.0.1 | - | 初始版本，支持ESP-IDF v5.5+ |