/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

/**
 * @file example_init_video.c
 * @brief 视频系统初始化模块
 *
 * 本文件实现了视频系统的初始化和反初始化功能，支持多种摄像头接口：
 * - MIPI-CSI：高速串行摄像头接口，支持高分辨率、高帧率
 * - DVP：并行摄像头接口，适用于中低分辨率场景
 * - SPI：串行外设接口，适用于低成本摄像头模块
 * - USB-UVC：USB 摄像头接口，支持标准 UVC 协议设备
 *
 * 核心功能：
 * 1. 根据编译配置选择启用的摄像头接口类型
 * 2. 配置各接口的硬件参数（I2C/SPI 端口、GPIO 引脚、时钟频率等）
 * 3. 初始化 SCCB(I2C) 总线（支持应用层初始化或组件内部初始化）
 * 4. 配置 XCLK 时钟（用于需要外部时钟的摄像头）
 * 5. 调用 esp_video_init() 完成底层驱动初始化
 *
 * 初始化流程：
 * ┌─────────────────────────────────────────────────────────────────────┐
 * │  example_video_init()                                               │
 * │      │                                                              │
 * │      ▼                                                              │
 * │  ┌─────────────────────────────────────────────────────────────┐     │
 * │  │  检查是否已初始化（s_is_init）                             │     │
 * │  └──────────────────────┬────────────────────────────────────┘     │
 * │                         │                                          │
 * │                         ▼                                          │
 * │  ┌─────────────────────────────────────────────────────────────┐     │
 * │  │  [可选] 应用层初始化 SCCB(I2C) 总线                        │     │
 * │  │  CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP                      │     │
 * │  └──────────────────────┬────────────────────────────────────┘     │
 * │                         │                                          │
 * │                         ▼                                          │
 * │  ┌─────────────────────────────────────────────────────────────┐     │
 * │  │  [可选] 配置并启动 MIPI-CSI XCLK 时钟                      │     │
 * │  │  (需要 EXAMPLE_MIPI_CSI_XCLK_PIN > 0)                     │     │
 * │  └──────────────────────┬────────────────────────────────────┘     │
 * │                         │                                          │
 * │                         ▼                                          │
 * │  ┌─────────────────────────────────────────────────────────────┐     │
 * │  │  输出各接口配置日志（I2C端口、引脚、频率）                  │     │
 * │  └──────────────────────┬────────────────────────────────────┘     │
 * │                         │                                          │
 * │                         ▼                                          │
 * │  ┌─────────────────────────────────────────────────────────────┐     │
 * │  │  调用 esp_video_init() 初始化视频系统                       │     │
 * │  │  根据配置启用 MIPI-CSI/DVP/SPI/USB-UVC 接口               │     │
 * │  └──────────────────────┬────────────────────────────────────┘     │
 * │                         │                                          │
 * │                         ▼                                          │
 * │                    设置 s_is_init = true                            │
 * └─────────────────────────────────────────────────────────────────────┘
 */

#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "example_video_common.h"
#include "esp_cam_sensor_xclk.h"

#if EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
/**
 * @var s_csi_config
 * @brief MIPI-CSI 摄像头传感器配置
 *
 * 包含 MIPI-CSI 接口的所有硬件配置参数：
 * - SCCB(I2C) 总线配置（端口、引脚、频率）
 * - 传感器复位引脚和电源引脚
 * - 可选的 LDO 初始化控制
 *
 * @note MIPI-CSI 接口支持高速串行传输，适合高分辨率摄像头。
 */
static const esp_video_init_csi_config_t s_csi_config = {
    .sccb_config = {
#if !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
        .init_sccb = true,              /**< 由组件内部初始化 SCCB 总线 */
        .i2c_config = {
            .port      = EXAMPLE_MIPI_CSI_SCCB_I2C_PORT,       /**< I2C 端口号 */
            .scl_pin   = EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN,    /**< SCL 引脚 */
            .sda_pin   = EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN,    /**< SDA 引脚 */
        },
#endif /* !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
        .freq = EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ,               /**< SCCB 时钟频率 */
    },
    .reset_pin = EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN,        /**< 传感器复位引脚 */
    .pwdn_pin  = EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN,         /**< 传感器电源控制引脚 */
#if CONFIG_EXAMPLE_MIPI_CSI_VIDEO_DEVICE_DONT_INIT_LDO
    .dont_init_ldo = true,             /**< 不初始化 LDO（由外部电路控制） */
#endif /* CONFIG_EXAMPLE_MIPI_CSI_VIDEO_DEVICE_DONT_INIT_LDO */
};

#if EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR
/**
 * @var s_cam_motor_config
 * @brief MIPI-CSI 摄像头马达/云台配置
 *
 * 用于控制摄像头马达（如自动对焦、光学防抖）的配置参数。
 */
static const esp_video_init_cam_motor_config_t s_cam_motor_config = {
    .sccb_config = {
#if !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
        .init_sccb = true,
        .i2c_config = {
            .port      = EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_PORT,
            .scl_pin   = EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_SCL_PIN,
            .sda_pin   = EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_SDA_PIN,
        },
#endif /* !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
        .freq      = EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_FREQ,
    },
    .reset_pin = EXAMPLE_MIPI_CSI_CAM_MOTOR_RESET_PIN,
    .pwdn_pin  = EXAMPLE_MIPI_CSI_CAM_MOTOR_PWDN_PIN,
    .signal_pin = EXAMPLE_MIPI_CSI_CAM_MOTOR_SIGNAL_PIN,
};
#endif /* EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR */
#endif /* EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */

#if EXAMPLE_ENABLE_DVP_CAM_SENSOR
/**
 * @var s_dvp_config
 * @brief DVP（Digital Video Port）摄像头传感器配置
 *
 * DVP 是并行接口，包含以下配置：
 * - SCCB(I2C) 总线配置
 * - 传感器复位和电源引脚
 * - DVP 并行接口引脚（数据总线、同步信号、时钟）
 * - XCLK 时钟频率
 *
 * @note DVP 接口使用并行数据传输，最高支持 8 位数据宽度，适合中低分辨率摄像头。
 */
static const esp_video_init_dvp_config_t s_dvp_config = {
    .sccb_config = {
#if !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
        .init_sccb = true,              /**< 由组件内部初始化 SCCB 总线 */
        .i2c_config = {
            .port      = EXAMPLE_DVP_SCCB_I2C_PORT,            /**< I2C 端口号 */
            .scl_pin   = EXAMPLE_DVP_SCCB_I2C_SCL_PIN,         /**< SCL 引脚 */
            .sda_pin   = EXAMPLE_DVP_SCCB_I2C_SDA_PIN,         /**< SDA 引脚 */
        },
#endif /* !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
        .freq      = EXAMPLE_DVP_SCCB_I2C_FREQ,                /**< SCCB 时钟频率 */
    },
    .reset_pin = EXAMPLE_DVP_CAM_SENSOR_RESET_PIN,             /**< 传感器复位引脚 */
    .pwdn_pin  = EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN,              /**< 传感器电源控制引脚 */
    .dvp_pin = {
        .data_width = CAM_CTLR_DATA_WIDTH_8,                   /**< 数据总线宽度：8 位 */
        .data_io = {
            EXAMPLE_DVP_D0_PIN, EXAMPLE_DVP_D1_PIN, EXAMPLE_DVP_D2_PIN, EXAMPLE_DVP_D3_PIN,
            EXAMPLE_DVP_D4_PIN, EXAMPLE_DVP_D5_PIN, EXAMPLE_DVP_D6_PIN, EXAMPLE_DVP_D7_PIN,
        },                                                     /**< 数据引脚 D0-D7 */
        .vsync_io = EXAMPLE_DVP_VSYNC_PIN,                     /**< 垂直同步信号引脚 */
        .de_io = EXAMPLE_DVP_DE_PIN,                           /**< 数据使能信号引脚 */
        .pclk_io = EXAMPLE_DVP_PCLK_PIN,                       /**< 像素时钟引脚 */
        .xclk_io = EXAMPLE_DVP_XCLK_PIN,                       /**< 外部时钟引脚 */
    },
    .xclk_freq = EXAMPLE_DVP_XCLK_FREQ,                        /**< XCLK 时钟频率 */
};
#endif /* EXAMPLE_ENABLE_DVP_CAM_SENSOR */

#if EXAMPLE_ENABLE_SPI_CAM_SENSOR
/**
 * @var s_spi_config
 * @brief SPI 摄像头传感器配置数组
 *
 * SPI 接口摄像头支持单摄像头和双摄像头配置：
 * - SPI CAM 0：主摄像头
 * - SPI CAM 1：副摄像头（可选）
 *
 * 配置参数包括：
 * - SCCB(I2C) 总线配置
 * - 传感器复位和电源引脚
 * - SPI 接口配置（端口、CS、SCLK、DATA0）
 * - XCLK 时钟配置（频率、资源类型、LEDC 参数）
 *
 * @note SPI 接口可选择标准 SPI 或 PARLIO（并行 IO）模式。
 */
static const esp_video_init_spi_config_t s_spi_config[] = {
    {
        .sccb_config = {
#if !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
            .init_sccb = true,              /**< 由组件内部初始化 SCCB 总线 */
            .i2c_config = {
                .port      = EXAMPLE_SPI_CAM_0_SCCB_I2C_PORT,  /**< I2C 端口号 */
                .scl_pin   = EXAMPLE_SPI_CAM_0_SCCB_I2C_SCL_PIN, /**< SCL 引脚 */
                .sda_pin   = EXAMPLE_SPI_CAM_0_SCCB_I2C_SDA_PIN, /**< SDA 引脚 */
            },
#endif /* !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
            .freq      = EXAMPLE_SPI_CAM_0_SCCB_I2C_FREQ,      /**< SCCB 时钟频率 */
        },
        .reset_pin = EXAMPLE_SPI_CAM_0_SENSOR_RESET_PIN,       /**< 传感器复位引脚 */
        .pwdn_pin  = EXAMPLE_SPI_CAM_0_SENSOR_PWDN_PIN,        /**< 传感器电源控制引脚 */

#if EXAMPLE_SPI_CAM_0_INTERFACE_PARLIO
        .intf = ESP_CAM_CTLR_SPI_CAM_INTF_PARLIO,              /**< 使用 PARLIO 接口 */
#else
        .intf = ESP_CAM_CTLR_SPI_CAM_INTF_SPI,                 /**< 使用标准 SPI 接口 */
#endif /* EXAMPLE_SPI_CAM_0_INTERFACE_PARLIO */

        .spi_port = EXAMPLE_SPI_CAM_0_SPI_PORT,                /**< SPI 端口号 */
        .spi_cs_pin = EXAMPLE_SPI_CAM_0_CS_PIN,                /**< SPI CS 引脚 */
        .spi_sclk_pin = EXAMPLE_SPI_CAM_0_SCLK_PIN,            /**< SPI SCLK 引脚 */
        .spi_data0_io_pin = EXAMPLE_SPI_CAM_0_DATA0_IO_PIN,     /**< SPI DATA0 IO 引脚 */

        .xclk_source = EXAMPLE_SPI_CAM_0_XCLK_RESOURCE,        /**< XCLK 时钟资源类型 */
        .xclk_pin = EXAMPLE_SPI_CAM_0_XCLK_PIN,                /**< XCLK 引脚 */
        .xclk_freq = EXAMPLE_SPI_CAM_0_XCLK_FREQ,              /**< XCLK 时钟频率 */
#if CONFIG_EXAMPLE_SPI_CAM_XCLK_USE_LEDC
        .xclk_ledc_cfg = {
            .timer = EXAMPLE_SPI_CAM_0_XCLK_TIMER,             /**< LEDC 定时器编号 */
            .clk_cfg = LEDC_AUTO_CLK,                         /**< LEDC 时钟配置 */
            .channel = EXAMPLE_SPI_CAM_0_XCLK_TIMER_CHANNEL,   /**< LEDC 通道编号 */
        },
#endif /* CONFIG_EXAMPLE_SPI_CAM_XCLK_USE_LEDC */
    },

#if EXAMPLE_ENABLE_SPI_CAM_1_SENSOR
    {
        .sccb_config = {
#if !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
            .init_sccb = true,
            .i2c_config = {
                .port = EXAMPLE_SPI_CAM_1_SCCB_I2C_PORT,
                .scl_pin = EXAMPLE_SPI_CAM_1_SCCB_I2C_SCL_PIN,
                .sda_pin = EXAMPLE_SPI_CAM_1_SCCB_I2C_SDA_PIN,
            },
#endif /* !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
            .freq = EXAMPLE_SPI_CAM_1_SCCB_I2C_FREQ,
        },
        .reset_pin = EXAMPLE_SPI_CAM_1_SENSOR_RESET_PIN,
        .pwdn_pin = EXAMPLE_SPI_CAM_1_SENSOR_PWDN_PIN,

        .intf = ESP_CAM_CTLR_SPI_CAM_INTF_SPI,
        .spi_port = EXAMPLE_SPI_CAM_1_SPI_PORT,
        .spi_cs_pin = EXAMPLE_SPI_CAM_1_CS_PIN,
        .spi_sclk_pin = EXAMPLE_SPI_CAM_1_SCLK_PIN,
        .spi_data0_io_pin = EXAMPLE_SPI_CAM_1_DATA0_IO_PIN,

        .xclk_source = EXAMPLE_SPI_CAM_1_XCLK_RESOURCE,
        .xclk_pin = EXAMPLE_SPI_CAM_1_XCLK_PIN,
        .xclk_freq = EXAMPLE_SPI_CAM_1_XCLK_FREQ,
#if CONFIG_EXAMPLE_SPI_CAM_1_XCLK_USE_LEDC
        .xclk_ledc_cfg = {
            .timer = EXAMPLE_SPI_CAM_1_XCLK_TIMER,
            .clk_cfg = LEDC_AUTO_CLK,
            .channel = EXAMPLE_SPI_CAM_1_XCLK_TIMER_CHANNEL,
        },
#endif /* CONFIG_EXAMPLE_SPI_CAM_1_XCLK_USE_LEDC */
    },
#endif /* EXAMPLE_ENABLE_SPI_CAM_1_SENSOR */
};
#endif /* EXAMPLE_ENABLE_SPI_CAM_SENSOR */

#if EXAMPLE_ENABLE_USB_UVC_CAM_SENSOR
/**
 * @var s_usb_uvc_config
 * @brief USB UVC 摄像头配置
 *
 * 包含 USB UVC 设备的所有配置参数：
 * - UVC 设备配置（数量、任务堆栈、优先级、CPU 亲和性）
 * - USB 主机库配置（初始化、外设映射、任务参数）
 *
 * @note USB UVC 支持标准 USB 摄像头设备，即插即用。
 */
static const esp_video_init_usb_uvc_config_t s_usb_uvc_config = {
    .uvc = {
        .uvc_dev_num = CONFIG_EXAMPLE_USB_UVC_DEVICES_NUM,     /**< UVC 设备数量 */
        .task_stack = CONFIG_EXAMPLE_USB_UVC_TASK_STACK_SIZE,  /**< UVC 任务堆栈大小 */
        .task_priority = CONFIG_EXAMPLE_USB_UVC_TASK_PRIORITY, /**< UVC 任务优先级 */
        .task_affinity = CONFIG_EXAMPLE_USB_UVC_TASK_AFFINITY, /**< UVC 任务 CPU 亲和性 */
    },
    .usb = {
        .init_usb_host_lib = true,              /**< 初始化 USB 主机库 */
        .peripheral_map = CONFIG_EXAMPLE_USB_PERIPHERAL_MAP,   /**< USB 外设映射 */
        .task_stack = CONFIG_EXAMPLE_USB_LIB_TASK_STACK_SIZE,  /**< USB 库任务堆栈大小 */
        .task_priority = CONFIG_EXAMPLE_USB_LIB_TASK_PRIORITY, /**< USB 库任务优先级 */
        .task_affinity = CONFIG_EXAMPLE_USB_LIB_TASK_AFFINITY, /**< USB 库任务 CPU 亲和性 */
    },
};
#endif /* EXAMPLE_ENABLE_USB_UVC_CAM_SENSOR */

/**
 * @var s_cam_config
 * @brief 视频系统主配置结构
 *
 * 整合所有摄像头接口的配置，根据编译配置选择启用的接口。
 * esp_video_init() 将根据此配置初始化对应的摄像头驱动。
 */
static const esp_video_init_config_t s_cam_config = {
#if EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
    .csi      = &s_csi_config,                /**< MIPI-CSI 配置指针（启用时） */
#if EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR
    .cam_motor = &s_cam_motor_config,         /**< 摄像头马达配置（启用时） */
#endif /* EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR */
#endif /* EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */
#if EXAMPLE_ENABLE_DVP_CAM_SENSOR
    .dvp      = &s_dvp_config,                /**< DVP 配置指针（启用时） */
#endif /* EXAMPLE_ENABLE_DVP_CAM_SENSOR */
#if EXAMPLE_ENABLE_SPI_CAM_SENSOR
    .spi      = s_spi_config,                 /**< SPI 配置数组（启用时） */
#endif /* EXAMPLE_ENABLE_SPI_CAM_SENSOR */
#if EXAMPLE_ENABLE_USB_UVC_CAM_SENSOR
    .usb_uvc  = &s_usb_uvc_config,            /**< USB UVC 配置指针（启用时） */
#endif /* EXAMPLE_ENABLE_USB_UVC_CAM_SENSOR */
};

#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
/**
 * @var s_bus_config
 * @brief 应用层初始化 SCCB(I2C) 总线配置
 *
 * 当 CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP 启用时，由应用层负责初始化 SCCB 总线，
 * 各摄像头接口共享同一个 I2C 总线实例。
 */
static const i2c_master_bus_config_t s_bus_config = {
    .i2c_port = EXAMPLE_SCCB_I2C_PORT_INIT_BY_APP,             /**< I2C 端口号 */
    .sda_io_num = EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP,        /**< SDA 引脚 */
    .scl_io_num = EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP,        /**< SCL 引脚 */
    .clk_source = I2C_CLK_SRC_DEFAULT,                         /**< 时钟源：默认 */
    .glitch_ignore_cnt = 7,                                    /**< 毛刺忽略计数 */
    .flags.enable_internal_pullup = true,                       /**< 启用内部上拉电阻 */
};

/**
 * @var s_i2cbus_handle
 * @brief SCCB(I2C) 总线句柄
 *
 * 应用层初始化的 I2C 总线实例句柄，供各摄像头接口共享使用。
 */
static i2c_master_bus_handle_t s_i2cbus_handle;
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

#if defined(EXAMPLE_MIPI_CSI_XCLK_PIN) && EXAMPLE_MIPI_CSI_XCLK_PIN > 0
/**
 * @var s_xclk_handle
 * @brief MIPI-CSI XCLK 时钟句柄
 *
 * 用于管理 MIPI-CSI 摄像头的外部时钟资源，需要在摄像头初始化前启动。
 */
static esp_cam_sensor_xclk_handle_t s_xclk_handle;
#endif /* defined(EXAMPLE_MIPI_CSI_XCLK_PIN) && EXAMPLE_MIPI_CSI_XCLK_PIN > 0 */

/**
 * @var s_is_init
 * @brief 视频系统初始化状态标志
 *
 * 防止重复初始化视频系统，确保初始化操作只执行一次。
 */
static bool s_is_init = false;

/**
 * @var TAG
 * @brief 日志标签
 */
static const char *TAG = "example_init_video";

/**
 * @brief 初始化视频系统
 *
 * 完成以下初始化步骤：
 * 1. 检查是否已初始化（防止重复初始化）
 * 2. [可选] 应用层初始化 SCCB(I2C) 总线
 * 3. [可选] 配置并启动 MIPI-CSI XCLK 时钟
 * 4. 输出各接口配置日志
 * 5. 调用 esp_video_init() 初始化视频系统
 * 6. 设置初始化完成标志
 *
 * @note 对于需要主机提供 XCLK 时钟的摄像头设备，此函数必须在设备重启后立即调用，
 *       否则摄像头可能因缺少主时钟而无法启动。
 *
 * @return ESP_OK 成功，其他错误码表示失败
 */
esp_err_t example_video_init(void)
{
    esp_err_t ret;

    /* 检查是否已初始化 */
    if (s_is_init) {
        return ESP_OK;
    }

    /* 默认使用静态配置 */
    const esp_video_init_config_t *cam_config_ptr = &s_cam_config;

#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
    /* 应用层初始化 SCCB(I2C) 总线 */
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&s_bus_config, &s_i2cbus_handle), TAG, "failed to initialize i2c bus");

#if EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
    /* MIPI-CSI：复制配置并设置使用外部 I2C 句柄 */
    esp_video_init_csi_config_t csi_config = s_csi_config;
    csi_config.sccb_config.init_sccb = false;           /**< 禁止组件内部初始化 SCCB */
    csi_config.sccb_config.i2c_handle = s_i2cbus_handle; /**< 使用应用层初始化的 I2C 句柄 */

#if EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR
    /* MIPI-CSI 马达：同样使用外部 I2C 句柄 */
    esp_video_init_cam_motor_config_t cam_motor_config = s_cam_motor_config;
    cam_motor_config.sccb_config.init_sccb = false;
    cam_motor_config.sccb_config.i2c_handle = s_i2cbus_handle;
#endif /* EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR */
#endif /* EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */

#if EXAMPLE_ENABLE_DVP_CAM_SENSOR
    /* DVP：复制配置并设置使用外部 I2C 句柄 */
    esp_video_init_dvp_config_t dvp_config = s_dvp_config;
    dvp_config.sccb_config.init_sccb = false;
    dvp_config.sccb_config.i2c_handle = s_i2cbus_handle;
#endif /* EXAMPLE_ENABLE_DVP_CAM_SENSOR */

#if EXAMPLE_ENABLE_SPI_CAM_0_SENSOR
    /* SPI：复制配置并设置使用外部 I2C 句柄 */
    esp_video_init_spi_config_t spi_config[ESP_VIDEO_SPI_DEVICE_NUM];

    memcpy(spi_config, s_spi_config, sizeof(esp_video_init_spi_config_t) * ESP_VIDEO_SPI_DEVICE_NUM);
    for (int i = 0; i < ESP_VIDEO_SPI_DEVICE_NUM; i++) {
        spi_config[i].sccb_config.init_sccb = false;
        spi_config[i].sccb_config.i2c_handle = s_i2cbus_handle;
    }
#endif /* EXAMPLE_ENABLE_SPI_CAM_0_SENSOR */

    /* 构建使用外部 I2C 句柄的配置结构 */
    esp_video_init_config_t cam_config = {
#if EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
        .csi      = &csi_config,
#if EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR
        .cam_motor = &cam_motor_config,
#endif /* EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR */
#endif /* EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */
#if EXAMPLE_ENABLE_DVP_CAM_SENSOR
        .dvp      = &dvp_config,
#endif /* EXAMPLE_ENABLE_DVP_CAM_SENSOR */
#if EXAMPLE_ENABLE_SPI_CAM_SENSOR
        .spi      = spi_config,
#endif /* EXAMPLE_ENABLE_SPI_CAM_SENSOR */
#if EXAMPLE_ENABLE_USB_UVC_CAM_SENSOR
        .usb_uvc  = &s_usb_uvc_config,
#endif /* EXAMPLE_ENABLE_USB_UVC_CAM_SENSOR */
    };

    /* 使用应用层配置 */
    cam_config_ptr = &cam_config;
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

#if defined(EXAMPLE_MIPI_CSI_XCLK_PIN) && EXAMPLE_MIPI_CSI_XCLK_PIN > 0
    /* 配置并启动 MIPI-CSI XCLK 时钟 */
    esp_cam_sensor_xclk_config_t cam_xclk_config = {
        .esp_clock_router_cfg = {
            .xclk_pin = EXAMPLE_MIPI_CSI_XCLK_PIN,             /**< XCLK 引脚 */
            .xclk_freq_hz = EXAMPLE_MIPI_CSI_XCLK_FREQ,        /**< XCLK 频率（Hz） */
        }
    };

    ESP_LOGI(TAG, "MIPI-CSI xclk pin=%d, freq=%d", EXAMPLE_MIPI_CSI_XCLK_PIN, EXAMPLE_MIPI_CSI_XCLK_FREQ);

    /* 分配 XCLK 时钟资源 */
    ESP_GOTO_ON_ERROR(esp_cam_sensor_xclk_allocate(ESP_CAM_SENSOR_XCLK_ESP_CLOCK_ROUTER, &s_xclk_handle), failed_0, TAG, "failed to allocate xclk");

    /* 启动 XCLK 时钟 */
    ESP_GOTO_ON_ERROR(esp_cam_sensor_xclk_start(s_xclk_handle, &cam_xclk_config), failed_1, TAG, "failed to start xclk");
#endif /* defined(EXAMPLE_MIPI_CSI_XCLK_PIN) && EXAMPLE_MIPI_CSI_XCLK_PIN > 0 */

    /* 输出各接口配置日志 */

#if EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
    ESP_LOGI(TAG, "MIPI-CSI camera sensor I2C port=%d, scl_pin=%d, sda_pin=%d, freq=%d",
             EXAMPLE_MIPI_CSI_SCCB_I2C_PORT,
             EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN,
             EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN,
             EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ);
#endif /* EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */

#if EXAMPLE_ENABLE_DVP_CAM_SENSOR
    ESP_LOGI(TAG, "DVP camera sensor I2C port=%d, scl_pin=%d, sda_pin=%d, freq=%d",
             EXAMPLE_DVP_SCCB_I2C_PORT,
             EXAMPLE_DVP_SCCB_I2C_SDA_PIN,
             EXAMPLE_DVP_SCCB_I2C_SDA_PIN,
             EXAMPLE_DVP_SCCB_I2C_FREQ);
#endif /* EXAMPLE_ENABLE_DVP_CAM_SENSOR */

#if EXAMPLE_ENABLE_SPI_CAM_0_SENSOR
    ESP_LOGI(TAG, "SPI camera sensor 0 I2C port=%d, scl_pin=%d, sda_pin=%d, freq=%d",
             EXAMPLE_SPI_CAM_0_SCCB_I2C_PORT,
             EXAMPLE_SPI_CAM_0_SCCB_I2C_SCL_PIN,
             EXAMPLE_SPI_CAM_0_SCCB_I2C_SDA_PIN,
             EXAMPLE_SPI_CAM_0_SCCB_I2C_FREQ);
#if EXAMPLE_ENABLE_SPI_CAM_1_SENSOR
    ESP_LOGI(TAG, "SPI camera sensor 1 I2C port=%d, scl_pin=%d, sda_pin=%d, freq=%d",
             EXAMPLE_SPI_CAM_1_SCCB_I2C_PORT,
             EXAMPLE_SPI_CAM_1_SCCB_I2C_SCL_PIN,
             EXAMPLE_SPI_CAM_1_SCCB_I2C_SDA_PIN,
             EXAMPLE_SPI_CAM_1_SCCB_I2C_FREQ);
#endif /* EXAMPLE_ENABLE_SPI_CAM_1_SENSOR */
#endif /* EXAMPLE_ENABLE_SPI_CAM_0_SENSOR */

    /* 初始化视频系统 */
    ESP_GOTO_ON_ERROR(esp_video_init(cam_config_ptr), failed_2, TAG, "failed to initialize video");

    /* 设置初始化完成标志 */
    s_is_init = true;

    return ESP_OK;

    /* 错误处理：按逆序释放资源 */

failed_2:
#if EXAMPLE_MIPI_CSI_XCLK_PIN > 0
    /* 停止 XCLK 时钟 */
    esp_cam_sensor_xclk_stop(s_xclk_handle);
failed_1:
    /* 释放 XCLK 时钟资源 */
    esp_cam_sensor_xclk_free(s_xclk_handle);
    s_xclk_handle = NULL;
failed_0:
#endif

#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
    /* 删除应用层初始化的 I2C 总线 */
    i2c_del_master_bus(s_i2cbus_handle);
    s_i2cbus_handle = NULL;
#endif

    return ret;
}

/**
 * @brief 反初始化视频系统
 *
 * 按逆序释放视频系统相关资源：
 * 1. 反初始化视频驱动（esp_video_deinit）
 * 2. 停止并释放 XCLK 时钟（如果已启动）
 * 3. 删除应用层初始化的 I2C 总线（如果已初始化）
 * 4. 重置初始化标志
 *
 * @return ESP_OK 成功，其他错误码表示失败
 */
esp_err_t example_video_deinit(void)
{
    esp_err_t ret = ESP_OK;

    /* 检查是否已初始化 */
    if (!s_is_init) {
        return ESP_OK;
    }

    /* 反初始化视频驱动 */
    ESP_RETURN_ON_ERROR(esp_video_deinit(), TAG, "failed to deinitialize video");

#if EXAMPLE_MIPI_CSI_XCLK_PIN > 0
    /* 停止 XCLK 时钟 */
    ESP_RETURN_ON_ERROR(esp_cam_sensor_xclk_stop(s_xclk_handle), TAG, "failed to stop xclk");

    /* 释放 XCLK 时钟资源 */
    ESP_RETURN_ON_ERROR(esp_cam_sensor_xclk_free(s_xclk_handle), TAG, "failed to free xclk");
    s_xclk_handle = NULL;
#endif

#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
    /* 删除应用层初始化的 I2C 总线 */
    ESP_RETURN_ON_ERROR(i2c_del_master_bus(s_i2cbus_handle), TAG, "failed to delete i2c bus");
    s_i2cbus_handle = NULL;
#endif

    /* 重置初始化标志 */
    s_is_init = false;

    return ret;
}
