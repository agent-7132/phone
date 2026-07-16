/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

/**
 * @file example_video_common_board.h
 * @brief 自定义开发板硬件配置文件模板
 *
 * 本文件是自定义开发板的配置模板，所有引脚参数均从 menuconfig 中读取。
 * 用户可以通过 `idf.py menuconfig` 配置自己的硬件引脚映射。
 *
 * 使用方法：
 * 1. 在 menuconfig 中选择 "Customized" 作为开发板类型
 * 2. 根据实际硬件连接配置各引脚参数
 * 3. 编译时将使用此处定义的配置
 */

#pragma once

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CUSTOMIZED_BOARD_CONFIG 自定义开发板配置
 * @brief 自定义开发板的硬件引脚和接口配置
 *
 * 所有配置参数均从 menuconfig 中读取，允许用户完全自定义硬件连接。
 * @{
 */

/**
 * @defgroup CUSTOMIZED_SCCB_CONFIG SCCB(I2C) 总线配置
 * @brief SCCB(I2C) 总线的引脚配置
 * @{
 */
#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
/**
 * @def EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP
 * @brief 应用层统一初始化 SCCB 总线时的 SCL 引脚
 *
 * 值从 menuconfig 的 CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP 读取。
 */
#define EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP            CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP
/**
 * @def EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP
 * @brief 应用层统一初始化 SCCB 总线时的 SDA 引脚
 *
 * 值从 menuconfig 的 CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP 读取。
 */
#define EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP            CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
/** @} */

/**
 * @defgroup CUSTOMIZED_MIPI_CSI_CONFIG MIPI-CSI 摄像头配置
 * @brief MIPI-CSI 接口摄像头的引脚配置
 * @{
 */
#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
/**
 * @def EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN
 * @brief MIPI-CSI 摄像头 SCCB 总线 SCL 引脚
 *
 * 如果启用了应用层统一初始化，使用统一的 SCL 引脚；否则从 menuconfig 读取。
 */
#if !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN               CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN
#define EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN               CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN
#else /* !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
#define EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN               EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP
#define EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN               EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP
#endif /* !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

/**
 * @def EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN
 * @brief MIPI-CSI 摄像头复位引脚
 *
 * 值从 menuconfig 的 CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN 读取。
 * 设置为 -1 表示不使用硬件复位。
 */
#define EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN           CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN
/**
 * @def EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN
 * @brief MIPI-CSI 摄像头电源使能引脚
 *
 * 值从 menuconfig 的 CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN 读取。
 * 设置为 -1 表示不使用硬件电源控制。
 */
#define EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN            CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN
/**
 * @def EXAMPLE_MIPI_CSI_XCLK_PIN
 * @brief MIPI-CSI 摄像头外部时钟引脚
 *
 * 值从 menuconfig 的 CONFIG_EXAMPLE_MIPI_CSI_XCLK_PIN 读取。
 * 设置为 -1 表示不使用外部时钟。
 */
#define EXAMPLE_MIPI_CSI_XCLK_PIN                       CONFIG_EXAMPLE_MIPI_CSI_XCLK_PIN
#if CONFIG_EXAMPLE_MIPI_CSI_XCLK_PIN >= 0
/**
 * @def EXAMPLE_MIPI_CSI_XCLK_FREQ
 * @brief MIPI-CSI 摄像头外部时钟频率
 *
 * 值从 menuconfig 的 CONFIG_EXAMPLE_MIPI_CSI_XCLK_FREQ 读取。
 * 仅当 XCLK_PIN 有效时才定义此宏。
 */
#define EXAMPLE_MIPI_CSI_XCLK_FREQ                      CONFIG_EXAMPLE_MIPI_CSI_XCLK_FREQ
#endif /* CONFIG_EXAMPLE_MIPI_CSI_XCLK_PIN >= 0 */

#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR
/**
 * @def EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_SCL_PIN
 * @brief MIPI-CSI 摄像头电机 SCCB 总线 SCL 引脚
 */
#if !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_SCL_PIN     CONFIG_EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_SCL_PIN
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_SDA_PIN     CONFIG_EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_SDA_PIN
#else /* !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_SCL_PIN     EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_SDA_PIN     EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP
#endif /* !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

/**
 * @def EXAMPLE_MIPI_CSI_CAM_MOTOR_RESET_PIN
 * @brief MIPI-CSI 摄像头电机复位引脚
 */
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_RESET_PIN            CONFIG_EXAMPLE_MIPI_CSI_CAM_MOTOR_RESET_PIN
/**
 * @def EXAMPLE_MIPI_CSI_CAM_MOTOR_PWDN_PIN
 * @brief MIPI-CSI 摄像头电机电源使能引脚
 */
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_PWDN_PIN             CONFIG_EXAMPLE_MIPI_CSI_CAM_MOTOR_PWDN_PIN
/**
 * @def EXAMPLE_MIPI_CSI_CAM_MOTOR_SIGNAL_PIN
 * @brief MIPI-CSI 摄像头电机控制信号引脚
 */
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_SIGNAL_PIN           CONFIG_EXAMPLE_MIPI_CSI_CAM_MOTOR_SIGNAL_PIN
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR */
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */
/** @} */

/**
 * @defgroup CUSTOMIZED_DVP_CONFIG DVP 摄像头配置
 * @brief DVP 接口摄像头的引脚配置
 * @{
 */
#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
/**
 * @def EXAMPLE_DVP_SCCB_I2C_SCL_PIN
 * @brief DVP 摄像头 SCCB 总线 SCL 引脚
 */
#if !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_DVP_SCCB_I2C_SCL_PIN                    CONFIG_EXAMPLE_DVP_SCCB_I2C_SCL_PIN
#define EXAMPLE_DVP_SCCB_I2C_SDA_PIN                    CONFIG_EXAMPLE_DVP_SCCB_I2C_SDA_PIN
#else /* !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
#define EXAMPLE_DVP_SCCB_I2C_SCL_PIN                    EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP
#define EXAMPLE_DVP_SCCB_I2C_SDA_PIN                    EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP
#endif /* !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

/**
 * @def EXAMPLE_DVP_CAM_SENSOR_RESET_PIN
 * @brief DVP 摄像头复位引脚
 */
#define EXAMPLE_DVP_CAM_SENSOR_RESET_PIN                CONFIG_EXAMPLE_DVP_CAM_SENSOR_RESET_PIN
/**
 * @def EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN
 * @brief DVP 摄像头电源使能引脚
 */
#define EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN                 CONFIG_EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN

/**
 * @defgroup CUSTOMIZED_DVP_PIN_CONFIG DVP 接口引脚配置
 * @brief DVP 接口的时序和数据引脚配置
 * @{
 */
/**
 * @def EXAMPLE_DVP_XCLK_PIN
 * @brief DVP 摄像头外部时钟引脚
 */
#define EXAMPLE_DVP_XCLK_PIN                            CONFIG_EXAMPLE_DVP_XCLK_PIN
/**
 * @def EXAMPLE_DVP_PCLK_PIN
 * @brief DVP 像素时钟引脚
 */
#define EXAMPLE_DVP_PCLK_PIN                            CONFIG_EXAMPLE_DVP_PCLK_PIN
/**
 * @def EXAMPLE_DVP_VSYNC_PIN
 * @brief DVP 垂直同步引脚
 */
#define EXAMPLE_DVP_VSYNC_PIN                           CONFIG_EXAMPLE_DVP_VSYNC_PIN
/**
 * @def EXAMPLE_DVP_DE_PIN
 * @brief DVP 数据使能引脚
 */
#define EXAMPLE_DVP_DE_PIN                              CONFIG_EXAMPLE_DVP_DE_PIN
/**
 * @def EXAMPLE_DVP_D0_PIN ~ EXAMPLE_DVP_D7_PIN
 * @brief DVP 8 位数据总线引脚
 */
#define EXAMPLE_DVP_D0_PIN                              CONFIG_EXAMPLE_DVP_D0_PIN
#define EXAMPLE_DVP_D1_PIN                              CONFIG_EXAMPLE_DVP_D1_PIN
#define EXAMPLE_DVP_D2_PIN                              CONFIG_EXAMPLE_DVP_D2_PIN
#define EXAMPLE_DVP_D3_PIN                              CONFIG_EXAMPLE_DVP_D3_PIN
#define EXAMPLE_DVP_D4_PIN                              CONFIG_EXAMPLE_DVP_D4_PIN
#define EXAMPLE_DVP_D5_PIN                              CONFIG_EXAMPLE_DVP_D5_PIN
#define EXAMPLE_DVP_D6_PIN                              CONFIG_EXAMPLE_DVP_D6_PIN
#define EXAMPLE_DVP_D7_PIN                              CONFIG_EXAMPLE_DVP_D7_PIN
/** @} */
#endif /* CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR */
/** @} */

/**
 * @defgroup CUSTOMIZED_SPI_CONFIG SPI 摄像头配置
 * @brief SPI 接口摄像头的引脚配置（支持单/双摄像头）
 * @{
 */
#if CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR
/**
 * @def EXAMPLE_SPI_CAM_0_SCCB_I2C_SCL_PIN
 * @brief SPI 摄像头 0 SCCB 总线 SCL 引脚
 */
#if !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_SPI_CAM_0_SCCB_I2C_SCL_PIN              CONFIG_EXAMPLE_SPI_CAM_0_SCCB_I2C_SCL_PIN
#define EXAMPLE_SPI_CAM_0_SCCB_I2C_SDA_PIN              CONFIG_EXAMPLE_SPI_CAM_0_SCCB_I2C_SDA_PIN
#else /* !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
#define EXAMPLE_SPI_CAM_0_SCCB_I2C_SCL_PIN              EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP
#define EXAMPLE_SPI_CAM_0_SCCB_I2C_SDA_PIN              EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP
#endif /* !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

/**
 * @def EXAMPLE_SPI_CAM_0_SENSOR_RESET_PIN
 * @brief SPI 摄像头 0 复位引脚
 */
#define EXAMPLE_SPI_CAM_0_SENSOR_RESET_PIN              CONFIG_EXAMPLE_SPI_CAM_0_SENSOR_RESET_PIN
/**
 * @def EXAMPLE_SPI_CAM_0_SENSOR_PWDN_PIN
 * @brief SPI 摄像头 0 电源使能引脚
 */
#define EXAMPLE_SPI_CAM_0_SENSOR_PWDN_PIN               CONFIG_EXAMPLE_SPI_CAM_0_SENSOR_PWDN_PIN

/**
 * @def EXAMPLE_SPI_CAM_0_XCLK_PIN
 * @brief SPI 摄像头 0 外部时钟引脚
 */
#define EXAMPLE_SPI_CAM_0_XCLK_PIN                      CONFIG_EXAMPLE_SPI_CAM_0_XCLK_PIN

/**
 * @defgroup CUSTOMIZED_SPI_0_PIN_CONFIG SPI 摄像头 0 接口引脚配置
 * @brief SPI 摄像头 0 的控制和数据引脚配置
 * @{
 */
/**
 * @def EXAMPLE_SPI_CAM_0_CS_PIN
 * @brief SPI 摄像头 0 片选引脚
 */
#define EXAMPLE_SPI_CAM_0_CS_PIN                        CONFIG_EXAMPLE_SPI_CAM_0_CS_PIN
/**
 * @def EXAMPLE_SPI_CAM_0_SCLK_PIN
 * @brief SPI 摄像头 0 时钟引脚
 */
#define EXAMPLE_SPI_CAM_0_SCLK_PIN                      CONFIG_EXAMPLE_SPI_CAM_0_SCLK_PIN
/**
 * @def EXAMPLE_SPI_CAM_0_DATA0_IO_PIN
 * @brief SPI 摄像头 0 双向数据引脚
 */
#define EXAMPLE_SPI_CAM_0_DATA0_IO_PIN                  CONFIG_EXAMPLE_SPI_CAM_0_DATA0_IO_PIN
/** @} */

#if CONFIG_EXAMPLE_ENABLE_SPI_CAM_1_SENSOR
/**
 * @def EXAMPLE_SPI_CAM_1_SCCB_I2C_SCL_PIN
 * @brief SPI 摄像头 1 SCCB 总线 SCL 引脚
 */
#if !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_SPI_CAM_1_SCCB_I2C_SCL_PIN              CONFIG_EXAMPLE_SPI_CAM_1_SCCB_I2C_SCL_PIN
#define EXAMPLE_SPI_CAM_1_SCCB_I2C_SDA_PIN              CONFIG_EXAMPLE_SPI_CAM_1_SCCB_I2C_SDA_PIN
#else /* !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
#define EXAMPLE_SPI_CAM_1_SCCB_I2C_SCL_PIN              EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP
#define EXAMPLE_SPI_CAM_1_SCCB_I2C_SDA_PIN              EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP
#endif /* !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

/**
 * @def EXAMPLE_SPI_CAM_1_SENSOR_RESET_PIN
 * @brief SPI 摄像头 1 复位引脚
 */
#define EXAMPLE_SPI_CAM_1_SENSOR_RESET_PIN              CONFIG_EXAMPLE_SPI_CAM_1_SENSOR_RESET_PIN
/**
 * @def EXAMPLE_SPI_CAM_1_SENSOR_PWDN_PIN
 * @brief SPI 摄像头 1 电源使能引脚
 */
#define EXAMPLE_SPI_CAM_1_SENSOR_PWDN_PIN               CONFIG_EXAMPLE_SPI_CAM_1_SENSOR_PWDN_PIN

/**
 * @def EXAMPLE_SPI_CAM_1_XCLK_PIN
 * @brief SPI 摄像头 1 外部时钟引脚
 */
#define EXAMPLE_SPI_CAM_1_XCLK_PIN                      CONFIG_EXAMPLE_SPI_CAM_1_XCLK_PIN

/**
 * @defgroup CUSTOMIZED_SPI_1_PIN_CONFIG SPI 摄像头 1 接口引脚配置
 * @brief SPI 摄像头 1 的控制和数据引脚配置
 * @{
 */
/**
 * @def EXAMPLE_SPI_CAM_1_CS_PIN
 * @brief SPI 摄像头 1 片选引脚
 */
#define EXAMPLE_SPI_CAM_1_CS_PIN                        CONFIG_EXAMPLE_SPI_CAM_1_CS_PIN
/**
 * @def EXAMPLE_SPI_CAM_1_SCLK_PIN
 * @brief SPI 摄像头 1 时钟引脚
 */
#define EXAMPLE_SPI_CAM_1_SCLK_PIN                      CONFIG_EXAMPLE_SPI_CAM_1_SCLK_PIN
/**
 * @def EXAMPLE_SPI_CAM_1_DATA0_IO_PIN
 * @brief SPI 摄像头 1 双向数据引脚
 */
#define EXAMPLE_SPI_CAM_1_DATA0_IO_PIN                  CONFIG_EXAMPLE_SPI_CAM_1_DATA0_IO_PIN
/** @} */
#endif /* CONFIG_EXAMPLE_ENABLE_SPI_CAM_1_SENSOR */
#endif /* CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR */
/** @} */

/** @} */

#ifdef __cplusplus
}
#endif
