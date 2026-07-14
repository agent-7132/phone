#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_ldo_regulator.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_st7102.h"
#include "esp_lcd_touch_st7123.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "hal/lcd_types.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#else
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#endif

#include "bsp.h"
#include "bsp_lcd_init_cmds.h"

/* ==================== 模块内部变量 ==================== */

/**
 * @brief 日志标签
 */
static const char *TAG = "BSP";

/**
 * @brief MIPI DSI总线句柄
 * 
 * 用于管理MIPI DSI硬件总线资源
 */
static esp_lcd_dsi_bus_handle_t s_mipi_dsi_bus = NULL;

/**
 * @brief MIPI PHY LDO通道句柄
 * 
 * 用于管理MIPI DSI PHY的2.5V电源
 */
static esp_ldo_channel_handle_t s_mipi_phy_ldo = NULL;

/**
 * @brief I2C总线句柄(用于触摸控制器)
 * 
 * 用于管理触摸控制器的I2C通信
 */
static i2c_master_bus_handle_t s_touch_i2c_bus = NULL;

/**
 * @brief 全局BSP句柄
 * 
 * 存储当前初始化的BSP句柄，通过bsp_display_get_handle()访问
 */
static bsp_handle_t s_bsp_handle = {0};

/**
 * @brief BSP初始化状态标志
 * 
 * 用于检查BSP是否已初始化
 */
static bool s_bsp_initialized = false;

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 根据显示旋转角度计算触摸坐标映射标志
 * 
 * 参考官方ESP-IDF示例项目中的触摸旋转辅助逻辑
 * 根据LCD旋转角度自动调整触摸坐标映射，确保触摸位置与显示内容对应
 * 
 * @param[in]  rotation  LVGL显示旋转角度
 * @param[out] swap_xy   是否交换X/Y轴
 * @param[out] mirror_x  是否镜像X轴
 * @param[out] mirror_y  是否镜像Y轴
 */
static void bsp_touch_get_rotation_flags(lv_disp_rotation_t rotation,
                                          bool *swap_xy, bool *mirror_x, bool *mirror_y)
{
    bool swap = false;
    bool x_mirror = false;
    bool y_mirror = false;

    switch (rotation) {
    case LV_DISP_ROTATION_90:
        swap = true;
        x_mirror = true;
        y_mirror = false;
        break;
    case LV_DISP_ROTATION_180:
        swap = false;
        x_mirror = true;
        y_mirror = true;
        break;
    case LV_DISP_ROTATION_270:
        swap = true;
        x_mirror = false;
        y_mirror = true;
        break;
    case LV_DISP_ROTATION_0:
    default:
        swap = false;
        x_mirror = false;
        y_mirror = false;
        break;
    }

    if (swap_xy != NULL) {
        *swap_xy = swap;
    }
    if (mirror_x != NULL) {
        *mirror_x = x_mirror;
    }
    if (mirror_y != NULL) {
        *mirror_y = y_mirror;
    }
}

/**
 * @brief 启用MIPI DSI PHY电源
 * 
 * MIPI DSI PHY需要2.5V供电, 通过内部LDO稳压器提供
 * 此函数必须在初始化MIPI DSI总线之前调用
 * 
 * @return 
 *      - ESP_OK: 成功
 *      - 其他: 失败
 */
static esp_err_t bsp_enable_mipi_phy_power(void)
{
    esp_err_t ret = ESP_OK;
    
    if (s_mipi_phy_ldo != NULL) {
        ESP_LOGD(TAG, "MIPI PHY LDO already acquired");
        return ESP_OK;
    }
    
    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = BSP_MIPI_DSI_PHY_LDO_CHAN,
        .voltage_mv = BSP_MIPI_DSI_PHY_VOLTAGE_MV,
    };
    
    ESP_GOTO_ON_ERROR(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &s_mipi_phy_ldo), 
                      err, TAG, "Failed to acquire MIPI PHY LDO channel");
    
    ESP_LOGI(TAG, "MIPI DSI PHY powered on (LDO_CHAN=%d, VOLTAGE=%dmV)", 
             BSP_MIPI_DSI_PHY_LDO_CHAN, BSP_MIPI_DSI_PHY_VOLTAGE_MV);
    
    return ESP_OK;

err:
    ESP_LOGE(TAG, "Failed to enable MIPI PHY power");
    return ret;
}

/**
 * @brief 初始化LCD背光GPIO
 * 
 * 配置背光控制引脚为输出模式
 * 如果背光引脚未使用(BSP_PIN_NUM_BK_LIGHT < 0), 则跳过初始化
 * 
 * @return 
 *      - ESP_OK: 成功
 *      - 其他: 失败
 */
static esp_err_t bsp_init_lcd_backlight_gpio(void)
{
#if BSP_PIN_NUM_BK_LIGHT >= 0
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << BSP_PIN_NUM_BK_LIGHT,
    };
    
    esp_err_t ret = gpio_config(&bk_gpio_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init backlight GPIO: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "LCD backlight GPIO initialized (PIN=%d)", BSP_PIN_NUM_BK_LIGHT);
#endif
    
    return ESP_OK;
}

/**
 * @brief 初始化触摸控制器使用的I2C总线
 * 
 * 创建I2C主机总线, 配置SDA/SCL引脚和时钟频率
 * 使用静态变量s_touch_i2c_bus确保总线只初始化一次
 * 
 * @return 
 *      - ESP_OK: 成功
 *      - 其他: 失败
 */
static esp_err_t bsp_init_touch_i2c(void)
{
    esp_err_t ret = ESP_OK;
    
    if (s_touch_i2c_bus != NULL) {
        ESP_LOGD(TAG, "Touch I2C bus already initialized");
        return ESP_OK;
    }
    
    const i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = BSP_TOUCH_I2C_NUM,
        .scl_io_num = BSP_TOUCH_I2C_SCL,
        .sda_io_num = BSP_TOUCH_I2C_SDA,
        .flags.enable_internal_pullup = true,
    };
    
    ESP_GOTO_ON_ERROR(i2c_new_master_bus(&i2c_bus_config, &s_touch_i2c_bus), 
                      err, TAG, "Failed to create touch I2C bus");
    
    ESP_LOGI(TAG, "Touch I2C bus initialized (SCL=%d, SDA=%d, CLK=%dkHz)", 
             BSP_TOUCH_I2C_SCL, BSP_TOUCH_I2C_SDA, BSP_TOUCH_I2C_CLK_HZ / 1000);
    
    return ESP_OK;

err:
    return ret;
}

/* ==================== 配置获取函数 ==================== */

bsp_display_cfg_t bsp_display_get_default_config(void)
{
    bsp_display_cfg_t cfg = {
        .rotation = LV_DISP_ROTATION_270,
        .touch_flags = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .enable_touch = true,
        .enable_backlight = true,
        .use_psram = true,
    };
    
    ESP_LOGD(TAG, "Returning default BSP display config: rotation=%d, touch=%d, backlight=%d, psram=%d",
             cfg.rotation, cfg.enable_touch, cfg.enable_backlight, cfg.use_psram);
    
    return cfg;
}

/* ==================== LCD初始化实现 ==================== */

esp_err_t bsp_lcd_init(esp_lcd_panel_handle_t *lcd_panel, esp_lcd_panel_io_handle_t *lcd_io)
{
    esp_err_t ret = ESP_OK;
    
    ESP_LOGI(TAG, "Initializing LCD panel (ST7102, MIPI-DSI, %dx%d)", 
             BSP_LCD_H_RES, BSP_LCD_V_RES);
    
    /* 1. 启用MIPI PHY电源 */
    ESP_GOTO_ON_ERROR(bsp_enable_mipi_phy_power(), err, TAG, "Failed to enable MIPI PHY power");
    
    /* 2. 初始化背光GPIO */
    ESP_GOTO_ON_ERROR(bsp_init_lcd_backlight_gpio(), err, TAG, "Failed to init backlight GPIO");
    
    /* 3. 关闭背光(初始化期间保持关闭) */
    ESP_GOTO_ON_ERROR(bsp_display_backlight_off(), err, TAG, "Failed to turn off backlight");
    
    /* 4. 创建MIPI DSI总线 */
    ESP_LOGI(TAG, "Creating MIPI DSI bus (bus_id=%d, lanes=%d, bit_rate=%dMbps)", 
             BSP_MIPI_DSI_BUS_ID, BSP_MIPI_DSI_NUM_LANES, BSP_MIPI_DSI_LANE_RATE_MBPS);
    
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = BSP_MIPI_DSI_BUS_ID,
        .num_data_lanes = BSP_MIPI_DSI_NUM_LANES,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = BSP_MIPI_DSI_LANE_RATE_MBPS,
    };
    
    ESP_GOTO_ON_ERROR(esp_lcd_new_dsi_bus(&bus_config, &s_mipi_dsi_bus), 
                      err, TAG, "Failed to create MIPI DSI bus");
    
    /* 5. 创建MIPI DBI IO(用于发送LCD命令) */
    ESP_LOGI(TAG, "Creating MIPI DBI panel IO");
    
    esp_lcd_dbi_io_config_t dbi_config = ST7102_MIPI_PANEL_IO_DBI_CONFIG();
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_dbi(s_mipi_dsi_bus, &dbi_config, lcd_io), 
                      err, TAG, "Failed to create MIPI DBI panel IO");
    
    /* 6. 创建ST7102控制面板 */
    ESP_LOGI(TAG, "Creating ST7102 LCD panel");
    
    esp_lcd_dpi_panel_config_t dpi_config = ST7102_MIPI_480_800_PANEL_60HZ_DPI_CONFIG(
        LCD_COLOR_FMT_RGB565);
    
    st7102_vendor_config_t vendor_config = {
        .init_cmds = bsp_lcd_init_cmds,
        .init_cmds_size = bsp_lcd_init_cmds_size,
        .flags.use_mipi_interface = 1,
        .mipi_config = {
            .dsi_bus = s_mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
    };
    
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BSP_LCD_COLOR_DEPTH,
        .vendor_config = &vendor_config,
    };
    
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7102(*lcd_io, &panel_config, lcd_panel), 
                      err, TAG, "Failed to create ST7102 panel");
    
    ESP_GOTO_ON_ERROR(esp_lcd_dpi_panel_enable_dma2d(*lcd_panel), 
                      err, TAG, "Failed to enable DMA2D for LCD panel");
    
    /* 7. 复位LCD面板 */
    ESP_LOGI(TAG, "Resetting LCD panel");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(*lcd_panel), err, TAG, "Failed to reset LCD panel");
    
    /* 8. 初始化LCD面板 */
    ESP_LOGI(TAG, "Initializing LCD panel");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(*lcd_panel), err, TAG, "Failed to init LCD panel");
    
    /* 9. 开启LCD显示 */
    ESP_LOGI(TAG, "Turning on LCD display");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_disp_on_off(*lcd_panel, true), 
                      err, TAG, "Failed to turn on LCD display");
    
    ESP_LOGI(TAG, "LCD panel initialization completed successfully");
    
    return ESP_OK;

err:
    /* 错误处理: 清理已分配的资源 */
    if (*lcd_panel != NULL) {
        esp_lcd_panel_del(*lcd_panel);
        *lcd_panel = NULL;
    }
    if (*lcd_io != NULL) {
        esp_lcd_panel_io_del(*lcd_io);
        *lcd_io = NULL;
    }
    if (s_mipi_dsi_bus != NULL) {
        esp_lcd_del_dsi_bus(s_mipi_dsi_bus);
        s_mipi_dsi_bus = NULL;
    }
    if (s_mipi_phy_ldo != NULL) {
        esp_ldo_release_channel(s_mipi_phy_ldo);
        s_mipi_phy_ldo = NULL;
    }
    
    ESP_LOGE(TAG, "LCD panel initialization failed");
    return ret;
}

/* ==================== 触摸初始化实现 ==================== */

esp_err_t bsp_touch_init(esp_lcd_touch_handle_t *touch)
{
    return bsp_touch_init_with_config(touch, false, false, false);
}

esp_err_t bsp_touch_init_with_config(esp_lcd_touch_handle_t *touch, 
                                     bool swap_xy, bool mirror_x, bool mirror_y)
{
    esp_err_t ret = ESP_OK;
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    
    ESP_LOGI(TAG, "Initializing touch controller (ST7123, I2C)");
    ESP_LOGI(TAG, "Touch config: swap_xy=%d, mirror_x=%d, mirror_y=%d", 
             swap_xy, mirror_x, mirror_y);
    
    /* 1. 初始化I2C总线 */
    ESP_GOTO_ON_ERROR(bsp_init_touch_i2c(), err, TAG, "Failed to init touch I2C bus");
    
    /* 2. 创建触摸面板IO */
    ESP_LOGI(TAG, "Creating touch panel IO");
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_ST7123_CONFIG();
    tp_io_config.scl_speed_hz = BSP_TOUCH_I2C_CLK_HZ;
    
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_i2c(s_touch_i2c_bus, &tp_io_config, &tp_io_handle), 
                      err, TAG, "Failed to create touch panel IO");
    
    /* 3. 配置触摸控制器参数 */
    ESP_LOGI(TAG, "Configuring touch controller");
    
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_H_RES,
        .y_max = BSP_LCD_V_RES,
        .rst_gpio_num = BSP_TOUCH_RST,
        .int_gpio_num = BSP_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
    };
    
    /* 4. 创建触摸控制器实例 */
    ESP_GOTO_ON_ERROR(esp_lcd_touch_new_i2c_st7123(tp_io_handle, &tp_cfg, touch), 
                      err, TAG, "Failed to create ST7123 touch controller");
    
    ESP_LOGI(TAG, "Touch controller initialization completed successfully");
    
    return ESP_OK;

err:
    ESP_LOGE(TAG, "Touch controller initialization failed");
    if (tp_io_handle != NULL) {
        esp_lcd_panel_io_del(tp_io_handle);
    }
    return ret;
}

/* ==================== LVGL初始化实现 ==================== */

esp_err_t bsp_lvgl_init(esp_lcd_panel_handle_t lcd_panel, esp_lcd_panel_io_handle_t lcd_io,
                        lv_disp_rotation_t rotation, bool use_psram, lv_display_t **disp)
{
    esp_err_t ret = ESP_OK;
    
    ESP_LOGI(TAG, "Initializing LVGL port");
    
    /* 1. 初始化LVGL核心 */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = BSP_LVGL_TASK_PRIORITY,
        .task_stack = BSP_LVGL_TASK_STACK_SIZE,
        .task_affinity = -1,
        .task_max_sleep_ms = BSP_LVGL_MAX_SLEEP_MS,
        .timer_period_ms = BSP_LVGL_TIMER_PERIOD_MS,
    };
    
    ESP_GOTO_ON_ERROR(lvgl_port_init(&lvgl_cfg), err, TAG, "Failed to init LVGL port");
    
    /* 2. 配置LVGL显示 */
    ESP_LOGI(TAG, "Adding LCD screen to LVGL (hres=%d, vres=%d, rotation=%d, psram=%d)", 
             BSP_LCD_H_RES, BSP_LCD_V_RES, rotation, use_psram);
    
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd_io,
        .panel_handle = lcd_panel,
        .buffer_size = BSP_LVGL_BUFFER_SIZE,
        .double_buffer = true,
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = (rotation == LV_DISP_ROTATION_90 || rotation == LV_DISP_ROTATION_270),
            .mirror_x = (rotation == LV_DISP_ROTATION_180 || rotation == LV_DISP_ROTATION_270),
            .mirror_y = (rotation == LV_DISP_ROTATION_180 || rotation == LV_DISP_ROTATION_90),
        },
        .flags = {
            .buff_dma = false,
            .buff_spiram = use_psram,
            .sw_rotate = true,
            .swap_bytes = false,
            .full_refresh = false,
            .direct_mode = false,
        },
    };
    
    const lvgl_port_display_dsi_cfg_t dpi_cfg = {
        .flags = {
            .avoid_tearing = false,
        },
    };
    
    /* 3. 添加显示到LVGL */
    *disp = lvgl_port_add_disp_dsi(&disp_cfg, &dpi_cfg);
    if (*disp == NULL) {
        ESP_LOGE(TAG, "Failed to add display to LVGL");
        return ESP_FAIL;
    }
    
    /* 4. 设置LVGL显示旋转 */
    lv_disp_set_rotation(*disp, rotation);
    
    ESP_LOGI(TAG, "LVGL initialization completed successfully");
    
    return ESP_OK;

err:
    ESP_LOGE(TAG, "LVGL initialization failed");
    return ret;
}

esp_err_t bsp_lvgl_add_touch(lv_display_t *disp, esp_lcd_touch_handle_t touch, lv_indev_t **indev)
{
    ESP_LOGI(TAG, "Adding touch input device to LVGL");
    
    if (disp == NULL || touch == NULL || indev == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }
    
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = touch,
    };
    
    *indev = lvgl_port_add_touch(&touch_cfg);
    if (*indev == NULL) {
        ESP_LOGE(TAG, "Failed to add touch input device");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Touch input device added successfully");
    
    return ESP_OK;
}

/* ==================== 完整初始化实现 ==================== */

esp_err_t bsp_display_start_with_config(const bsp_display_cfg_t *cfg, bsp_handle_t *handle)
{
    esp_err_t ret = ESP_OK;
    
    ESP_LOGI(TAG, "==================== BSP INIT START ====================");
    
    /* 参数检查 */
    if (cfg == NULL || handle == NULL) {
        ESP_LOGE(TAG, "Invalid arguments: cfg=%p, handle=%p", cfg, handle);
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 检查是否已初始化 */
    if (s_bsp_initialized) {
        ESP_LOGW(TAG, "BSP already initialized, returning existing handle");
        if (handle != NULL) {
            *handle = s_bsp_handle;
        }
        return ESP_OK;
    }
    
    /* 清零句柄 */
    memset(handle, 0, sizeof(bsp_handle_t));
    
    /* 1. 初始化LCD */
    ESP_LOGI(TAG, "Step 1/3: Initializing LCD...");
    ESP_GOTO_ON_ERROR(bsp_lcd_init(&handle->lcd_panel, &handle->lcd_io), 
                      err, TAG, "BSP init failed at LCD init");
    
    /* 2. 初始化LVGL */
    ESP_LOGI(TAG, "Step 2/3: Initializing LVGL...");
    ESP_GOTO_ON_ERROR(bsp_lvgl_init(handle->lcd_panel, handle->lcd_io, 
                                     cfg->rotation, cfg->use_psram, &handle->lvgl_disp), 
                      err, TAG, "BSP init failed at LVGL init");
    
    /* 3. 初始化触摸(可选) */
    if (cfg->enable_touch) {
        ESP_LOGI(TAG, "Step 3/3: Initializing touch...");
        
        bool swap_xy = cfg->touch_flags.swap_xy;
        bool mirror_x = cfg->touch_flags.mirror_x;
        bool mirror_y = cfg->touch_flags.mirror_y;
        
        if (!swap_xy && !mirror_x && !mirror_y) {
            bsp_touch_get_rotation_flags(cfg->rotation, &swap_xy, &mirror_x, &mirror_y);
            ESP_LOGI(TAG, "Auto-calculated touch rotation flags: swap_xy=%d, mirror_x=%d, mirror_y=%d",
                     swap_xy, mirror_x, mirror_y);
        } else {
            ESP_LOGI(TAG, "Using custom touch rotation flags: swap_xy=%d, mirror_x=%d, mirror_y=%d",
                     swap_xy, mirror_x, mirror_y);
        }
        
        ESP_GOTO_ON_ERROR(bsp_touch_init_with_config(&handle->touch, 
                                                      swap_xy,
                                                      mirror_x,
                                                      mirror_y), 
                          err, TAG, "BSP init failed at touch init");
        
        ESP_GOTO_ON_ERROR(bsp_lvgl_add_touch(handle->lvgl_disp, handle->touch, &handle->lvgl_touch), 
                          err, TAG, "BSP init failed at touch add");
    } else {
        ESP_LOGI(TAG, "Step 3/3: Touch disabled, skipping");
    }
    
    /* 4. 开启背光(可选) */
    if (cfg->enable_backlight) {
        ESP_LOGI(TAG, "Turning on backlight...");
        ESP_GOTO_ON_ERROR(bsp_display_backlight_on(), err, TAG, "Failed to turn on backlight");
    } else {
        ESP_LOGI(TAG, "Backlight disabled, keeping off");
    }
    
    /* 保存全局句柄 */
    memcpy(&s_bsp_handle, handle, sizeof(bsp_handle_t));
    s_bsp_initialized = true;
    
    ESP_LOGI(TAG, "==================== BSP INIT COMPLETE ====================");
    ESP_LOGI(TAG, "LCD Panel: ST7102, Resolution: %dx%d, Interface: MIPI-DSI", 
             BSP_LCD_H_RES, BSP_LCD_V_RES);
    ESP_LOGI(TAG, "Touch Controller: %s, Interface: I2C", 
             cfg->enable_touch ? "ST7123" : "Disabled");
    ESP_LOGI(TAG, "LVGL: Initialized with rotation %d", cfg->rotation);
    ESP_LOGI(TAG, "PSRAM Buffer: %s", cfg->use_psram ? "Enabled" : "Disabled");
    
    return ESP_OK;

err:
    /* 错误处理: 清理已分配的资源 */
    ESP_LOGE(TAG, "BSP initialization failed");
    
    if (handle->lvgl_touch != NULL) {
        lv_indev_delete(handle->lvgl_touch);
    }
    if (handle->lvgl_disp != NULL) {
        lv_display_delete(handle->lvgl_disp);
    }
    if (handle->touch != NULL) {
        esp_lcd_touch_del(handle->touch);
    }
    if (handle->lcd_panel != NULL) {
        esp_lcd_panel_del(handle->lcd_panel);
    }
    if (handle->lcd_io != NULL) {
        esp_lcd_panel_io_del(handle->lcd_io);
    }
    if (s_mipi_dsi_bus != NULL) {
        esp_lcd_del_dsi_bus(s_mipi_dsi_bus);
        s_mipi_dsi_bus = NULL;
    }
    if (s_mipi_phy_ldo != NULL) {
        esp_ldo_release_channel(s_mipi_phy_ldo);
        s_mipi_phy_ldo = NULL;
    }
    
    return ret;
}

esp_err_t bsp_display_start(bsp_handle_t *handle)
{
    bsp_display_cfg_t cfg = bsp_display_get_default_config();
    return bsp_display_start_with_config(&cfg, handle);
}

/* ==================== 背光控制实现 ==================== */

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_set_backlight(BSP_LCD_BK_LIGHT_ON_LEVEL);
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_set_backlight(BSP_LCD_BK_LIGHT_OFF_LEVEL);
}

esp_err_t bsp_display_set_backlight(uint32_t level)
{
#if BSP_PIN_NUM_BK_LIGHT >= 0
    esp_err_t ret = gpio_set_level(BSP_PIN_NUM_BK_LIGHT, level);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set backlight level: %s", esp_err_to_name(ret));
        return ret;
    }
#endif
    
    ESP_LOGD(TAG, "LCD backlight level set to %d", level);
    
    return ESP_OK;
}

/* ==================== 分辨率查询实现 ==================== */

void bsp_display_get_resolution(uint32_t *h_res, uint32_t *v_res)
{
    if (h_res != NULL) {
        *h_res = BSP_LCD_H_RES;
    }
    if (v_res != NULL) {
        *v_res = BSP_LCD_V_RES;
    }
}

/* ==================== 句柄获取实现 ==================== */

bsp_handle_t *bsp_display_get_handle(void)
{
    if (!s_bsp_initialized) {
        ESP_LOGW(TAG, "BSP not initialized, returning NULL");
        return NULL;
    }
    
    return &s_bsp_handle;
}

void bsp_display_lock(uint32_t timeout_ms)
{
    lvgl_port_lock(timeout_ms);
}

void bsp_display_unlock(void)
{
    lvgl_port_unlock();
}