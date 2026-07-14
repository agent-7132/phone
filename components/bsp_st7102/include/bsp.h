#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== BSP 硬件配置常量 ==================== */

/**
 * @brief LCD 屏幕分辨率定义
 * 
 * 屏幕规格: 4.3英寸 TFT, 分辨率 480x800, MIPI接口, ST7102驱动芯片
 */
#define BSP_LCD_H_RES               (480)    /* LCD水平分辨率 */
#define BSP_LCD_V_RES               (800)    /* LCD垂直分辨率 */
#define BSP_LCD_COLOR_DEPTH         (16)     /* 颜色深度: 16位RGB565 */

/**
 * @brief MIPI DSI PHY 电源配置
 * 
 * VDD_MIPI_DPHY 需要 2.5V 供电, 使用内部LDO稳压器
 */
#define BSP_MIPI_DSI_PHY_LDO_CHAN   (3)      /* LDO通道号: LDO_VO3 */
#define BSP_MIPI_DSI_PHY_VOLTAGE_MV (2500)   /* LDO输出电压: 2.5V */

/**
 * @brief LCD背光控制
 * 
 * 背光引脚配置, -1表示不使用GPIO控制背光
 */
#define BSP_LCD_BK_LIGHT_ON_LEVEL   (1)      /* 背光开启电平 */
#define BSP_LCD_BK_LIGHT_OFF_LEVEL  (0)      /* 背光关闭电平 */
#define BSP_PIN_NUM_BK_LIGHT        (-1)     /* 背光控制引脚(-1=未使用) */
#define BSP_PIN_NUM_LCD_RST         (-1)     /* LCD复位引脚(-1=未使用) */

/**
 * @brief 触摸控制器配置
 * 
 * ST7123触摸芯片通过I2C总线连接
 */
#define BSP_TOUCH_I2C_NUM           (0)      /* I2C端口号 */
#define BSP_TOUCH_I2C_CLK_HZ        (400000) /* I2C时钟频率: 400kHz */
#define BSP_TOUCH_I2C_SCL           (GPIO_NUM_8)  /* I2C SCL引脚 */
#define BSP_TOUCH_I2C_SDA           (GPIO_NUM_7)  /* I2C SDA引脚 */
#define BSP_TOUCH_RST               (GPIO_NUM_NC) /* 触摸复位引脚(未连接) */
#define BSP_TOUCH_INT               (GPIO_NUM_NC) /* 触摸中断引脚(未连接) */

/**
 * @brief LVGL任务配置
 */
#define BSP_LVGL_TASK_PRIORITY      (4)      /* LVGL任务优先级 */
#define BSP_LVGL_TASK_STACK_SIZE    (4096 * 2) /* LVGL任务堆栈大小 */
#define BSP_LVGL_TIMER_PERIOD_MS    (5)      /* LVGL定时器周期(ms) */
#define BSP_LVGL_MAX_SLEEP_MS       (500)    /* LVGL任务最大休眠时间(ms) */

/**
 * @brief MIPI DSI 总线配置
 */
#define BSP_MIPI_DSI_BUS_ID         (0)      /* DSI总线ID */
#define BSP_MIPI_DSI_NUM_LANES      (2)      /* 数据通道数量 */
#define BSP_MIPI_DSI_LANE_RATE_MBPS (820)    /* 单通道比特率(Mbps) */

/**
 * @brief LVGL显示缓冲区配置
 * 
 * 缓冲区大小 = 水平分辨率 * 垂直分辨率 * 1字节(RGB565格式)
 * 使用双缓冲模式提升显示流畅度
 */
#define BSP_LVGL_BUFFER_SIZE        (BSP_LCD_H_RES * BSP_LCD_V_RES)

/* ==================== BSP 配置结构体 ==================== */

/**
 * @brief BSP显示配置结构体
 * 
 * 参考官方BSP风格的配置方式，支持旋转、防撕裂模式、触摸标志等配置
 */
typedef struct {
    /**
     * @brief LVGL显示旋转角度
     * 
     * 支持: LV_DISP_ROTATION_0, LV_DISP_ROTATION_90, 
     *       LV_DISP_ROTATION_180, LV_DISP_ROTATION_270
     */
    lv_disp_rotation_t rotation;
    
    /**
     * @brief 触摸配置标志
     */
    struct {
        /**
         * @brief 是否交换X/Y轴
         * 
         * 当屏幕物理方向与逻辑方向不一致时使用
         */
        bool swap_xy;
        
        /**
         * @brief 是否镜像X轴
         * 
         * 水平翻转显示
         */
        bool mirror_x;
        
        /**
         * @brief 是否镜像Y轴
         * 
         * 垂直翻转显示
         */
        bool mirror_y;
    } touch_flags;
    
    /**
     * @brief 是否启用触摸功能
     * 
     * 设置为false可跳过触摸初始化
     */
    bool enable_touch;
    
    /**
     * @brief 是否启用背光
     * 
     * 设置为false可保持背光关闭
     */
    bool enable_backlight;
    
    /**
     * @brief 是否使用PSRAM存储显示缓冲区
     * 
     * 建议启用以节省内部SRAM
     */
    bool use_psram;
} bsp_display_cfg_t;

/**
 * @brief BSP句柄结构体
 * 
 * 封装了所有硬件相关的句柄, 提供统一的硬件访问接口
 */
typedef struct {
    esp_lcd_panel_handle_t    lcd_panel;    /* LCD面板句柄 */
    esp_lcd_panel_io_handle_t lcd_io;       /* LCD IO句柄 */
    esp_lcd_touch_handle_t    touch;        /* 触摸控制器句柄 */
    lv_display_t             *lvgl_disp;    /* LVGL显示句柄 */
    lv_indev_t               *lvgl_touch;   /* LVGL触摸输入设备句柄 */
} bsp_handle_t;

/* ==================== BSP 公共API ==================== */

/**
 * @brief 获取BSP显示默认配置
 * 
 * 返回一个预配置的bsp_display_cfg_t结构体，包含常用的默认值
 * 
 * @return 默认配置结构体
 */
bsp_display_cfg_t bsp_display_get_default_config(void);

/**
 * @brief 初始化LCD显示模块
 * 
 * 完成MIPI DSI总线初始化、LDO电源配置、ST7102面板初始化
 * 
 * @param[out] lcd_panel  返回LCD面板句柄
 * @param[out] lcd_io     返回LCD IO句柄
 * @return 
 *      - ESP_OK: 成功
 *      - 其他: 失败
 */
esp_err_t bsp_lcd_init(esp_lcd_panel_handle_t *lcd_panel, esp_lcd_panel_io_handle_t *lcd_io);

/**
 * @brief 初始化触摸控制器
 * 
 * 完成I2C总线初始化、ST7123触摸芯片初始化
 * 
 * @param[out] touch  返回触摸控制器句柄
 * @return 
 *      - ESP_OK: 成功
 *      - 其他: 失败
 */
esp_err_t bsp_touch_init(esp_lcd_touch_handle_t *touch);

/**
 * @brief 使用配置初始化触摸控制器
 * 
 * 完成I2C总线初始化、ST7123触摸芯片初始化，支持自定义触摸标志
 * 
 * @param[out] touch      返回触摸控制器句柄
 * @param[in]  swap_xy    是否交换X/Y轴
 * @param[in]  mirror_x   是否镜像X轴
 * @param[in]  mirror_y   是否镜像Y轴
 * @return 
 *      - ESP_OK: 成功
 *      - 其他: 失败
 */
esp_err_t bsp_touch_init_with_config(esp_lcd_touch_handle_t *touch, 
                                     bool swap_xy, bool mirror_x, bool mirror_y);

/**
 * @brief 初始化LVGL显示适配器
 * 
 * 将LCD面板注册到LVGL, 创建显示缓冲区和刷新任务
 * 
 * @param[in]  lcd_panel  LCD面板句柄
 * @param[in]  lcd_io     LCD IO句柄
 * @param[in]  rotation   LVGL显示旋转角度
 * @param[in]  use_psram  是否使用PSRAM
 * @param[out] disp       返回LVGL显示句柄
 * @return 
 *      - ESP_OK: 成功
 *      - 其他: 失败
 */
esp_err_t bsp_lvgl_init(esp_lcd_panel_handle_t lcd_panel, esp_lcd_panel_io_handle_t lcd_io,
                        lv_disp_rotation_t rotation, bool use_psram, lv_display_t **disp);

/**
 * @brief 注册触摸输入设备到LVGL
 * 
 * 将触摸控制器注册为LVGL输入设备
 * 
 * @param[in]  disp   LVGL显示句柄
 * @param[in]  touch  触摸控制器句柄
 * @param[out] indev  返回LVGL输入设备句柄
 * @return 
 *      - ESP_OK: 成功
 *      - 其他: 失败
 */
esp_err_t bsp_lvgl_add_touch(lv_display_t *disp, esp_lcd_touch_handle_t touch, lv_indev_t **indev);

/**
 * @brief 使用配置初始化整个BSP系统
 * 
 * 一次性完成LCD、触摸、LVGL的初始化，支持自定义配置
 * 
 * @param[in]  cfg     BSP显示配置
 * @param[out] handle  返回BSP句柄
 * @return 
 *      - ESP_OK: 成功
 *      - 其他: 失败
 */
esp_err_t bsp_display_start_with_config(const bsp_display_cfg_t *cfg, bsp_handle_t *handle);

/**
 * @brief 使用默认配置初始化整个BSP系统
 * 
 * 一次性完成LCD、触摸、LVGL的初始化，使用默认配置
 * 
 * @param[out] handle  返回BSP句柄
 * @return 
 *      - ESP_OK: 成功
 *      - 其他: 失败
 */
esp_err_t bsp_display_start(bsp_handle_t *handle);

/**
 * @brief 启用LCD背光
 * 
 * @return 
 *      - ESP_OK: 成功
 *      - 其他: 失败
 */
esp_err_t bsp_display_backlight_on(void);

/**
 * @brief 禁用LCD背光
 * 
 * @return 
 *      - ESP_OK: 成功
 *      - 其他: 失败
 */
esp_err_t bsp_display_backlight_off(void);

/**
 * @brief 设置LCD背光亮度
 * 
 * @param[in] level 背光电平(0=关闭, 1=开启)
 * @return 
 *      - ESP_OK: 成功
 *      - 其他: 失败
 */
esp_err_t bsp_display_set_backlight(uint32_t level);

/**
 * @brief 获取LCD分辨率信息
 * 
 * @param[out] h_res 水平分辨率
 * @param[out] v_res 垂直分辨率
 */
void bsp_display_get_resolution(uint32_t *h_res, uint32_t *v_res);

/**
 * @brief 获取当前BSP句柄
 * 
 * 返回当前初始化的BSP句柄，方便在其他地方访问硬件资源
 * 
 * @return BSP句柄指针，未初始化返回NULL
 */
bsp_handle_t *bsp_display_get_handle(void);

/**
 * @brief 锁定LVGL显示上下文
 * 
 * 在访问LVGL API之前调用此函数，确保线程安全
 * 
 * @param timeout_ms 等待锁的超时时间(ms)，-1表示无限等待
 */
void bsp_display_lock(uint32_t timeout_ms);

/**
 * @brief 解锁LVGL显示上下文
 * 
 * 在完成LVGL API调用后调用此函数
 */
void bsp_display_unlock(void);

#ifdef __cplusplus
}
#endif