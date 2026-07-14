# ESP32-P4 BSP - ST7102 LCD + ST7123 Touch

## 概述

本BSP组件封装了**4.3英寸TFT屏幕**（分辨率480×800，MIPI-DSI接口，ST7102驱动芯片）的显示、触摸和LVGL适配功能，提供简洁统一的API接口，便于快速集成到ESP32-P4项目中。

**ESP-IDF版本要求**: >= 5.5（兼容v6.0+）

**关键特性**:
- ✅ MIPI-DSI 2通道高速显示接口
- ✅ ST7102 LCD控制器驱动
- ✅ ST7123电容触摸屏控制器驱动
- ✅ LVGL v9.x图形库适配
- ✅ PSRAM显示缓冲区支持
- ✅ 自动触摸旋转映射
- ✅ 完善的错误处理和资源清理

## 硬件规格

| 参数 | 说明 |
|------|------|
| 屏幕尺寸 | 4.3英寸 TFT |
| 分辨率 | 480 × 800 |
| LCD驱动芯片 | ST7102 |
| 触摸芯片 | ST7123 |
| 接口 | MIPI-DSI (2通道) + I2C |
| 颜色深度 | 16位 (RGB565) |
| 开发板 | ESP32-P4 |

## 引脚配置

### MIPI-DSI接口

MIPI-DSI使用ESP32-P4内置的DSI控制器，无需额外配置引脚。

### I2C触摸接口

| 信号 | ESP32-P4引脚 | 说明 |
|------|-------------|------|
| I2C_SCL | GPIO8 | 触摸I2C时钟线 |
| I2C_SDA | GPIO7 | 触摸I2C数据线 |

### 背光控制

| 信号 | ESP32-P4引脚 | 说明 |
|------|-------------|------|
| BK_LIGHT | -1 (未使用) | 背光控制引脚，默认未使用 |
| LCD_RST | -1 (未使用) | LCD复位引脚，默认未使用 |

## 配置常量

所有硬件配置常量定义在 `bsp.h` 中，可根据实际硬件进行修改：

### LCD配置

```c
#define BSP_LCD_H_RES               (480)    // 水平分辨率
#define BSP_LCD_V_RES               (800)    // 垂直分辨率
#define BSP_LCD_COLOR_DEPTH         (16)     // 颜色深度(RGB565)
```

### MIPI DSI配置

```c
#define BSP_MIPI_DSI_BUS_ID         (0)      // DSI总线ID
#define BSP_MIPI_DSI_NUM_LANES      (2)      // 数据通道数量
#define BSP_MIPI_DSI_LANE_RATE_MBPS (820)    // 单通道比特率(Mbps)
```

### MIPI PHY电源配置

```c
#define BSP_MIPI_DSI_PHY_LDO_CHAN   (3)      // LDO通道号(LDO_VO3)
#define BSP_MIPI_DSI_PHY_VOLTAGE_MV (2500)   // LDO输出电压(2.5V)
```

### 触摸配置

```c
#define BSP_TOUCH_I2C_NUM           (0)      // I2C端口号
#define BSP_TOUCH_I2C_CLK_HZ        (400000) // I2C时钟频率
#define BSP_TOUCH_I2C_SCL           (GPIO_NUM_8)
#define BSP_TOUCH_I2C_SDA           (GPIO_NUM_7)
#define BSP_TOUCH_RST               (GPIO_NUM_NC)  // 触摸复位(未连接)
#define BSP_TOUCH_INT               (GPIO_NUM_NC)  // 触摸中断(未连接)
```

### LVGL配置

```c
#define BSP_LVGL_TASK_PRIORITY      (4)      // LVGL任务优先级
#define BSP_LVGL_TASK_STACK_SIZE    (4096 * 2) // LVGL任务堆栈大小
#define BSP_LVGL_TIMER_PERIOD_MS    (5)      // LVGL定时器周期(ms)
#define BSP_LVGL_MAX_SLEEP_MS       (500)    // LVGL任务最大休眠时间(ms)
#define BSP_LVGL_BUFFER_SIZE        (BSP_LCD_H_RES * BSP_LCD_V_RES)
```

## API接口

### 数据类型

#### bsp_display_cfg_t

BSP显示配置结构体：

```c
typedef struct {
    lv_disp_rotation_t rotation;           // LVGL显示旋转角度
    struct {
        bool swap_xy;                       // 是否交换X/Y轴
        bool mirror_x;                      // 是否镜像X轴
        bool mirror_y;                      // 是否镜像Y轴
    } touch_flags;                          // 触摸坐标映射标志
    bool enable_touch;                      // 是否启用触摸功能
    bool enable_backlight;                  // 是否启用背光
    bool use_psram;                         // 是否使用PSRAM存储显示缓冲区
} bsp_display_cfg_t;
```

**默认配置**:
- rotation: `LV_DISP_ROTATION_270`
- touch_flags: 全部为false（自动计算模式）
- enable_touch: true
- enable_backlight: true
- use_psram: true

#### bsp_handle_t

BSP句柄结构体，封装了所有硬件相关的句柄：

```c
typedef struct {
    esp_lcd_panel_handle_t    lcd_panel;    // LCD面板句柄
    esp_lcd_panel_io_handle_t lcd_io;       // LCD IO句柄
    esp_lcd_touch_handle_t    touch;        // 触摸控制器句柄
    lv_display_t             *lvgl_disp;    // LVGL显示句柄
    lv_indev_t               *lvgl_touch;   // LVGL触摸输入设备句柄
} bsp_handle_t;
```

### 函数接口

#### bsp_display_get_default_config()

**功能**：获取BSP显示默认配置

**原型**：
```c
bsp_display_cfg_t bsp_display_get_default_config(void);
```

**返回值**：默认配置结构体 `bsp_display_cfg_t`

---

#### bsp_display_start_with_config()

**功能**：使用自定义配置初始化整个BSP系统（LCD + 触摸 + LVGL）

**原型**：
```c
esp_err_t bsp_display_start_with_config(const bsp_display_cfg_t *cfg, bsp_handle_t *handle);
```

**参数**：
- `cfg`: BSP显示配置结构体指针
- `handle`: BSP句柄指针（输出参数）

**返回值**：
- `ESP_OK`: 初始化成功
- `ESP_ERR_INVALID_ARG`: 参数无效
- 其他错误码：初始化失败

**特性**：
- 支持自动计算触摸旋转标志（当touch_flags全部为false时）
- 支持多次调用（已初始化时返回已有的句柄）
- 完善的错误处理和资源清理

**示例**：
```c
bsp_display_cfg_t cfg = bsp_display_get_default_config();
cfg.rotation = LV_DISP_ROTATION_90;
cfg.enable_touch = true;

bsp_handle_t handle;
esp_err_t ret = bsp_display_start_with_config(&cfg, &handle);
```

---

#### bsp_display_start()

**功能**：使用默认配置初始化整个BSP系统

**原型**：
```c
esp_err_t bsp_display_start(bsp_handle_t *handle);
```

**参数**：
- `handle`: BSP句柄指针（输出参数）

**返回值**：
- `ESP_OK`: 初始化成功
- 其他错误码：初始化失败

**示例**：
```c
bsp_handle_t handle;
bsp_display_start(&handle);
```

---

#### bsp_lcd_init()

**功能**：初始化LCD显示模块

**原型**：
```c
esp_err_t bsp_lcd_init(esp_lcd_panel_handle_t *lcd_panel, esp_lcd_panel_io_handle_t *lcd_io);
```

**参数**：
- `lcd_panel`: 返回LCD面板句柄
- `lcd_io`: 返回LCD IO句柄

**返回值**：
- `ESP_OK`: 成功
- 其他错误码：失败

**内部流程**：
1. 启用MIPI PHY电源（LDO 2.5V）
2. 初始化背光GPIO
3. 关闭背光（初始化期间）
4. 创建MIPI DSI总线
5. 创建MIPI DBI面板IO
6. 创建ST7102控制面板
7. 复位LCD面板
8. 初始化LCD面板
9. 开启LCD显示

---

#### bsp_touch_init()

**功能**：初始化触摸控制器（ST7123），使用默认配置

**原型**：
```c
esp_err_t bsp_touch_init(esp_lcd_touch_handle_t *touch);
```

**参数**：
- `touch`: 返回触摸控制器句柄

**返回值**：
- `ESP_OK`: 成功
- 其他错误码：失败

---

#### bsp_touch_init_with_config()

**功能**：使用自定义配置初始化触摸控制器（ST7123）

**原型**：
```c
esp_err_t bsp_touch_init_with_config(esp_lcd_touch_handle_t *touch, 
                                     bool swap_xy, bool mirror_x, bool mirror_y);
```

**参数**：
- `touch`: 返回触摸控制器句柄
- `swap_xy`: 是否交换X/Y轴
- `mirror_x`: 是否镜像X轴
- `mirror_y`: 是否镜像Y轴

**返回值**：
- `ESP_OK`: 成功
- 其他错误码：失败

**内部流程**：
1. 初始化I2C总线（400kHz）
2. 创建触摸面板IO
3. 配置触摸控制器参数
4. 创建ST7123触摸控制器实例

---

#### bsp_lvgl_init()

**功能**：初始化LVGL显示适配器

**原型**：
```c
esp_err_t bsp_lvgl_init(esp_lcd_panel_handle_t lcd_panel, esp_lcd_panel_io_handle_t lcd_io,
                        lv_disp_rotation_t rotation, bool use_psram, lv_display_t **disp);
```

**参数**：
- `lcd_panel`: LCD面板句柄
- `lcd_io`: LCD IO句柄
- `rotation`: LVGL显示旋转角度
- `use_psram`: 是否使用PSRAM存储显示缓冲区
- `disp`: 返回LVGL显示句柄

**返回值**：
- `ESP_OK`: 成功
- 其他错误码：失败

**内部流程**：
1. 初始化LVGL核心（创建任务和定时器）
2. 配置LVGL显示（双缓冲、PSRAM缓冲、RGB565格式）
3. 添加显示到LVGL
4. 设置显示旋转角度

---

#### bsp_lvgl_add_touch()

**功能**：注册触摸输入设备到LVGL

**原型**：
```c
esp_err_t bsp_lvgl_add_touch(lv_display_t *disp, esp_lcd_touch_handle_t touch, lv_indev_t **indev);
```

**参数**：
- `disp`: LVGL显示句柄
- `touch`: 触摸控制器句柄
- `indev`: 返回LVGL输入设备句柄

**返回值**：
- `ESP_OK`: 成功
- 其他错误码：失败

---

#### bsp_display_backlight_on()

**功能**：启用LCD背光

**原型**：
```c
esp_err_t bsp_display_backlight_on(void);
```

**返回值**：
- `ESP_OK`: 成功

---

#### bsp_display_backlight_off()

**功能**：禁用LCD背光

**原型**：
```c
esp_err_t bsp_display_backlight_off(void);
```

**返回值**：
- `ESP_OK`: 成功

---

#### bsp_display_set_backlight()

**功能**：设置LCD背光亮度

**原型**：
```c
esp_err_t bsp_display_set_backlight(uint32_t level);
```

**参数**：
- `level`: 背光电平（0=关闭，1=开启）

**返回值**：
- `ESP_OK`: 成功

---

#### bsp_display_get_resolution()

**功能**：获取LCD分辨率信息

**原型**：
```c
void bsp_display_get_resolution(uint32_t *h_res, uint32_t *v_res);
```

**参数**：
- `h_res`: 返回水平分辨率
- `v_res`: 返回垂直分辨率

---

#### bsp_display_get_handle()

**功能**：获取当前BSP句柄

**原型**：
```c
bsp_handle_t *bsp_display_get_handle(void);
```

**返回值**：
- BSP句柄指针，未初始化返回NULL

---

#### bsp_display_lock()

**功能**：锁定LVGL显示上下文，确保线程安全

**原型**：
```c
void bsp_display_lock(uint32_t timeout_ms);
```

**参数**：
- `timeout_ms`: 等待锁的超时时间(ms)，-1表示无限等待

---

#### bsp_display_unlock()

**功能**：解锁LVGL显示上下文

**原型**：
```c
void bsp_display_unlock(void);
```

---

## 使用示例

### 方式一：一站式初始化（推荐）

```c
#include "bsp.h"
#include "lv_demos.h"
#include "freertos/FreeRTOS.h"

static bsp_handle_t s_bsp;
static const char *TAG = "MAIN";

void app_main(void) {
    bsp_display_cfg_t cfg = bsp_display_get_default_config();
    
    cfg.rotation = LV_DISP_ROTATION_270;
    cfg.enable_touch = true;
    cfg.use_psram = true;
    
    esp_err_t ret = bsp_display_start_with_config(&cfg, &s_bsp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BSP init failed: %s", esp_err_to_name(ret));
        return;
    }
    
    bsp_display_lock(-1);
    lv_demo_widgets();
    bsp_display_unlock();
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### 方式二：使用默认配置快速初始化

```c
#include "bsp.h"

void app_main(void) {
    bsp_handle_t handle;
    
    bsp_display_start(&handle);
    
    bsp_display_lock(-1);
    lv_demo_widgets();
    bsp_display_unlock();
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### 方式三：分步初始化

```c
#include "bsp.h"

void app_main(void) {
    esp_lcd_panel_handle_t lcd_panel;
    esp_lcd_panel_io_handle_t lcd_io;
    esp_lcd_touch_handle_t touch;
    lv_display_t *disp;
    lv_indev_t *touch_indev;
    
    bsp_lcd_init(&lcd_panel, &lcd_io);
    
    bsp_lvgl_init(lcd_panel, lcd_io, LV_DISP_ROTATION_270, true, &disp);
    
    bsp_touch_init(&touch);
    
    bsp_lvgl_add_touch(disp, touch, &touch_indev);
    
    bsp_display_backlight_on();
}
```

## 初始化流程图

```
bsp_display_start_with_config()
    │
    ├── 参数检查和初始化状态检查
    │
    ├── bsp_lcd_init()
    │       ├── bsp_enable_mipi_phy_power()
    │       ├── bsp_init_lcd_backlight_gpio()
    │       ├── bsp_display_backlight_off()
    │       ├── esp_lcd_new_dsi_bus()
    │       ├── esp_lcd_new_panel_io_dbi()
    │       ├── esp_lcd_new_panel_st7102()
    │       ├── esp_lcd_panel_reset()
    │       ├── esp_lcd_panel_init()
    │       └── esp_lcd_panel_disp_on_off()
    │
    ├── bsp_lvgl_init()
    │       ├── lvgl_port_init()
    │       └── lvgl_port_add_disp_dsi()
    │
    ├── bsp_touch_init_with_config() (可选)
    │       ├── bsp_touch_get_rotation_flags() (自动计算)
    │       ├── bsp_init_touch_i2c()
    │       ├── esp_lcd_new_panel_io_i2c()
    │       └── esp_lcd_touch_new_i2c_st7123()
    │
    ├── bsp_lvgl_add_touch() (可选)
    │       └── lvgl_port_add_touch()
    │
    └── bsp_display_backlight_on() (可选)
```

## 触摸旋转映射说明

本BSP支持根据显示旋转角度自动计算触摸坐标映射标志，确保触摸位置与显示内容对应：

| 旋转角度 | swap_xy | mirror_x | mirror_y |
|---------|---------|----------|----------|
| 0° | false | false | false |
| 90° | true | true | false |
| 180° | false | true | true |
| 270° | true | false | true |

如果需要自定义触摸映射，可以在配置中手动设置touch_flags。

## ESP-IDF版本兼容性

### v6.0+ 变更

| 变更项 | v5.x | v6.x |
|--------|------|------|
| I2C头文件 | `driver/i2c_master.h` | `i2c_master.h` |
| GPIO头文件 | `driver/gpio.h` | `esp_driver_gpio/gpio.h` |
| FreeRTOS头文件 | 自动包含 | 需显式包含 `freertos/FreeRTOS.h` |
| 组件依赖 | `driver` | `esp_driver_i2c`, `esp_driver_gpio` |

### 版本支持范围

| ESP-IDF版本 | 状态 |
|------------|------|
| v5.5.x | 支持 |
| v6.0.x | 支持 |
| v6.x.x | 支持 |

## 组件依赖

| 组件 | 版本要求 | 说明 |
|------|---------|------|
| esp_lcd | 内置 | ESP-IDF LCD驱动框架 |
| esp_lvgl_port | >=2.7 | LVGL ESP-IDF适配器 |
| esp_lcd_st7102 | >=1.0 | ST7102 LCD驱动 |
| esp_lcd_touch_st7123 | >=1.0 | ST7123触摸驱动 |
| esp_ldo_regulator | 内置 | LDO稳压器驱动 |
| i2c_master | 内置 | I2C主机驱动 |
| esp_driver_gpio | 内置 | GPIO驱动 |
| lvgl/lvgl | >=9.0 | LVGL图形库 |

## 文件结构

```
components/bsp_st7102/
├── include/
│   ├── bsp.h              # BSP核心头文件(API声明+配置常量)
│   └── bsp_lcd_init_cmds.h # ST7102 LCD初始化命令数组
├── CMakeLists.txt         # CMake配置文件
├── README.md              # BSP说明文档
├── bsp.c                  # BSP实现文件
└── idf_component.yml      # 组件依赖声明
```

## 注意事项

1. **电源配置**：MIPI DSI PHY需要2.5V供电，通过ESP32-P4内部LDO通道3提供
2. **PSRAM要求**：建议使用PSRAM存储LVGL显示缓冲区，需要在sdkconfig中启用SPIRAM
3. **颜色深度**：当前配置为RGB565（16位），可根据需要修改为RGB888（24位）
4. **触摸校准**：ST7123触摸芯片出厂已校准，一般不需要额外校准
5. **错误处理**：所有函数都有完善的错误处理和日志输出，便于调试
6. **线程安全**：访问LVGL API前必须调用 `bsp_display_lock()`，完成后调用 `bsp_display_unlock()`

## 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| 1.0.0 | 2026-07 | 初始版本，支持ESP-IDF v5.5+和v6.0+ |