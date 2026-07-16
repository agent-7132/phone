/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

/**
 * @file example_video_common_board.h
 * @brief ESP32-S3-EYE 开发板硬件配置文件
 *
 * ESP32-S3-EYE 是 Espressif 官方推出的 AI 视觉开发板，搭载 ESP32-S3 芯片，
 * 支持 DVP 和 SPI 两种摄像头接口。本文件定义了该开发板的硬件引脚映射。
 */

#pragma once

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ESP32_S3_EYE_CONFIG ESP32-S3-EYE 开发板配置
 * @brief ESP32-S3-EYE 开发板的硬件引脚和接口配置
 * @{
 */

/**
 * @defgroup ESP32_S3_EYE_SCCB_CONFIG SCCB(I2C) 总线配置
 * @brief SCCB(I2C) 总线的引脚配置
 *
 * ESP32-S3-EYE 使用 GPIO5(SCL) 和 GPIO4(SDA) 作为 SCCB 总线引脚。
 * @{
 */
#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
/**
 * @def EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP
 * @brief 应用层统一初始化 SCCB 总线时的 SCL 引脚（GPIO5）
 */
#define EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP            5
/**
 * @def EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP
 * @brief 应用层统一初始化 SCCB 总线时的 SDA 引脚（GPIO4）
 */
#define EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP            4
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
/** @} */

/**
 * @defgroup ESP32_S3_EYE_DVP_CONFIG DVP 摄像头配置
 * @brief DVP 接口摄像头的引脚配置
 *
 * ESP32-S3-EYE 支持通过 DVP 接口连接摄像头传感器（如 OV2640）。
 * @{
 */
#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
/**
 * @def EXAMPLE_DVP_SCCB_I2C_SCL_PIN
 * @brief DVP 摄像头 SCCB 总线 SCL 引脚（GPIO5）
 */
#define EXAMPLE_DVP_SCCB_I2C_SCL_PIN                    5
/**
 * @def EXAMPLE_DVP_SCCB_I2C_SDA_PIN
 * @brief DVP 摄像头 SCCB 总线 SDA 引脚（GPIO4）
 */
#define EXAMPLE_DVP_SCCB_I2C_SDA_PIN                    4
/**
 * @def EXAMPLE_DVP_CAM_SENSOR_RESET_PIN
 * @brief DVP 摄像头复位引脚（-1 表示不使用硬件复位）
 */
#define EXAMPLE_DVP_CAM_SENSOR_RESET_PIN                -1
/**
 * @def EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN
 * @brief DVP 摄像头电源使能引脚（-1 表示不使用硬件电源控制）
 */
#define EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN                 -1

/**
 * @defgroup ESP32_S3_EYE_DVP_PIN_CONFIG DVP 接口引脚配置
 * @brief DVP 接口的时序和数据引脚配置
 * @{
 */
/**
 * @def EXAMPLE_DVP_XCLK_PIN
 * @brief DVP 摄像头外部时钟引脚（GPIO15）
 */
#define EXAMPLE_DVP_XCLK_PIN                            15
/**
 * @def EXAMPLE_DVP_PCLK_PIN
 * @brief DVP 像素时钟引脚（GPIO13）
 */
#define EXAMPLE_DVP_PCLK_PIN                            13
/**
 * @def EXAMPLE_DVP_VSYNC_PIN
 * @brief DVP 垂直同步引脚（GPIO6）
 */
#define EXAMPLE_DVP_VSYNC_PIN                           6
/**
 * @def EXAMPLE_DVP_DE_PIN
 * @brief DVP 数据使能引脚（GPIO7）
 */
#define EXAMPLE_DVP_DE_PIN                              7
/**
 * @def EXAMPLE_DVP_D0_PIN ~ EXAMPLE_DVP_D7_PIN
 * @brief DVP 8 位数据总线引脚
 */
#define EXAMPLE_DVP_D0_PIN                              11
#define EXAMPLE_DVP_D1_PIN                              9
#define EXAMPLE_DVP_D2_PIN                              8
#define EXAMPLE_DVP_D3_PIN                              10
#define EXAMPLE_DVP_D4_PIN                              12
#define EXAMPLE_DVP_D5_PIN                              18
#define EXAMPLE_DVP_D6_PIN                              17
#define EXAMPLE_DVP_D7_PIN                              16
/** @} */
#endif /* CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR */
/** @} */

/**
 * @defgroup ESP32_S3_EYE_SPI_CONFIG SPI 摄像头配置
 * @brief SPI 接口摄像头的引脚配置
 * @{
 */
#if CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR
/**
 * @def EXAMPLE_SPI_CAM_0_SCCB_I2C_SCL_PIN
 * @brief SPI 摄像头 0 SCCB 总线 SCL 引脚（GPIO5）
 */
#define EXAMPLE_SPI_CAM_0_SCCB_I2C_SCL_PIN              5
/**
 * @def EXAMPLE_SPI_CAM_0_SCCB_I2C_SDA_PIN
 * @brief SPI 摄像头 0 SCCB 总线 SDA 引脚（GPIO4）
 */
#define EXAMPLE_SPI_CAM_0_SCCB_I2C_SDA_PIN              4
/**
 * @def EXAMPLE_SPI_CAM_0_SENSOR_RESET_PIN
 * @brief SPI 摄像头 0 复位引脚（-1 表示不使用硬件复位）
 */
#define EXAMPLE_SPI_CAM_0_SENSOR_RESET_PIN              -1
/**
 * @def EXAMPLE_SPI_CAM_0_SENSOR_PWDN_PIN
 * @brief SPI 摄像头 0 电源使能引脚（-1 表示不使用硬件电源控制）
 */
#define EXAMPLE_SPI_CAM_0_SENSOR_PWDN_PIN               -1

/**
 * @defgroup ESP32_S3_EYE_SPI_PIN_CONFIG SPI 接口引脚配置
 * @brief SPI 接口的控制和数据引脚配置
 * @{
 */
/**
 * @def EXAMPLE_SPI_CAM_0_CS_PIN
 * @brief SPI 摄像头 0 片选引脚（GPIO6）
 */
#define EXAMPLE_SPI_CAM_0_CS_PIN                        6
/**
 * @def EXAMPLE_SPI_CAM_0_SCLK_PIN
 * @brief SPI 摄像头 0 时钟引脚（GPIO13）
 */
#define EXAMPLE_SPI_CAM_0_SCLK_PIN                      13
/**
 * @def EXAMPLE_SPI_CAM_0_DATA0_IO_PIN
 * @brief SPI 摄像头 0 双向数据引脚（GPIO16）
 */
#define EXAMPLE_SPI_CAM_0_DATA0_IO_PIN                  16
/** @} */

/**
 * @def EXAMPLE_SPI_CAM_0_XCLK_PIN
 * @brief SPI 摄像头 0 外部时钟引脚（GPIO15）
 */
#define EXAMPLE_SPI_CAM_0_XCLK_PIN                      15
#endif /* CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR */
/** @} */

/** @} */

#ifdef __cplusplus
}
#endif
