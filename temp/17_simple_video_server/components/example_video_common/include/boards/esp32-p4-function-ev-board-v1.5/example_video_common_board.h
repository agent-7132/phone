/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

/**
 * @file example_video_common_board.h
 * @brief ESP32-P4-Function-EV-Board V1.5 开发板硬件配置文件
 *
 * ESP32-P4-Function-EV-Board V1.5 是 Espressif 官方推出的功能验证开发板，
 * 支持 MIPI-CSI、DVP、SPI 三种摄像头接口。本文件定义了该开发板的硬件引脚映射。
 *
 * @note MIPI-CSI、DVP、SPI 接口共享 SCCB 总线（GPIO7/8），因此必须使用相同的 I2C 端口。
 */

#pragma once

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ESP32_P4_FUNCTION_EV_V15_CONFIG ESP32-P4-Function-EV-Board V1.5 配置
 * @brief ESP32-P4-Function-EV-Board V1.5 开发板的硬件引脚和接口配置
 * @{
 */

/**
 * @defgroup ESP32_P4_FUNCTION_EV_V15_SCCB_CONFIG SCCB(I2C) 总线配置
 * @brief SCCB(I2C) 总线的引脚配置和约束检查
 *
 * ESP32-P4-Function-EV-Board V1.5 的 MIPI-CSI、DVP、SPI 接口共享同一组 SCCB 引脚（GPIO7/8），
 * 因此所有摄像头接口必须使用相同的 I2C 端口。
 * @{
 */
#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
/**
 * @def EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP
 * @brief 应用层统一初始化 SCCB 总线时的 SCL 引脚（GPIO8）
 */
#define EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP            8
/**
 * @def EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP
 * @brief 应用层统一初始化 SCCB 总线时的 SDA 引脚（GPIO7）
 */
#define EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP            7
#else /* !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
/**
 * @brief MIPI-CSI 和 DVP SCCB I2C 端口必须相同的约束检查
 */
#if defined(CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT) && defined(CONFIG_EXAMPLE_DVP_SCCB_I2C_PORT)
#if CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT != CONFIG_EXAMPLE_DVP_SCCB_I2C_PORT
#error "MIPI-CSI and DVP SCCB I2C port must be the same"
#endif /* CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT != CONFIG_EXAMPLE_DVP_SCCB_I2C_PORT */
#endif /* CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT && CONFIG_EXAMPLE_DVP_SCCB_I2C_PORT */

/**
 * @brief MIPI-CSI 和 SPI SCCB I2C 端口必须相同的约束检查
 */
#if defined(CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT) && defined(CONFIG_EXAMPLE_SPI_SCCB_I2C_PORT)
#if CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT != CONFIG_EXAMPLE_SPI_SCCB_I2C_PORT
#error "MIPI-CSI and SPI SCCB I2C port must be the same"
#endif /* CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT != CONFIG_EXAMPLE_SPI_SCCB_I2C_PORT */
#endif /* CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT && CONFIG_EXAMPLE_SPI_SCCB_I2C_PORT */

/**
 * @brief DVP 和 SPI SCCB I2C 端口必须相同的约束检查
 */
#if defined(CONFIG_EXAMPLE_DVP_SCCB_I2C_PORT) && defined(CONFIG_EXAMPLE_SPI_SCCB_I2C_PORT)
#if CONFIG_EXAMPLE_DVP_SCCB_I2C_PORT != CONFIG_EXAMPLE_SPI_SCCB_I2C_PORT
#error "DVP and SPI SCCB I2C port must be the same"
#endif /* CONFIG_EXAMPLE_DVP_SCCB_I2C_PORT != CONFIG_EXAMPLE_SPI_SCCB_I2C_PORT */
#endif /* CONFIG_EXAMPLE_DVP_SCCB_I2C_PORT && CONFIG_EXAMPLE_SPI_SCCB_I2C_PORT */
#endif /* !CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
/** @} */

/**
 * @defgroup ESP32_P4_FUNCTION_EV_V15_MIPI_CSI_CONFIG MIPI-CSI 摄像头配置
 * @brief MIPI-CSI 接口摄像头的引脚配置
 * @{
 */
#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
/**
 * @def EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN
 * @brief MIPI-CSI 摄像头 SCCB 总线 SCL 引脚（GPIO8）
 */
#define EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN               8
/**
 * @def EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN
 * @brief MIPI-CSI 摄像头 SCCB 总线 SDA 引脚（GPIO7）
 */
#define EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN               7
/**
 * @def EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN
 * @brief MIPI-CSI 摄像头复位引脚（-1 表示不使用硬件复位）
 */
#define EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN           -1
/**
 * @def EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN
 * @brief MIPI-CSI 摄像头电源使能引脚（-1 表示不使用硬件电源控制）
 */
#define EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN            -1
/**
 * @def EXAMPLE_MIPI_CSI_XCLK_PIN
 * @brief MIPI-CSI 摄像头外部时钟引脚（-1 表示不使用外部时钟）
 */
#define EXAMPLE_MIPI_CSI_XCLK_PIN                       -1

#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR
/**
 * @def EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_SCL_PIN
 * @brief MIPI-CSI 摄像头电机 SCCB 总线 SCL 引脚（GPIO8）
 */
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_SCL_PIN     8
/**
 * @def EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_SDA_PIN
 * @brief MIPI-CSI 摄像头电机 SCCB 总线 SDA 引脚（GPIO7）
 */
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_SDA_PIN     7

/**
 * @def EXAMPLE_MIPI_CSI_CAM_MOTOR_RESET_PIN
 * @brief MIPI-CSI 摄像头电机复位引脚（-1 表示不使用）
 */
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_RESET_PIN            -1
/**
 * @def EXAMPLE_MIPI_CSI_CAM_MOTOR_PWDN_PIN
 * @brief MIPI-CSI 摄像头电机电源使能引脚（-1 表示不使用）
 */
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_PWDN_PIN             -1
/**
 * @def EXAMPLE_MIPI_CSI_CAM_MOTOR_SIGNAL_PIN
 * @brief MIPI-CSI 摄像头电机控制信号引脚（-1 表示不使用）
 */
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_SIGNAL_PIN           -1
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR */
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */
/** @} */

/**
 * @defgroup ESP32_P4_FUNCTION_EV_V15_DVP_CONFIG DVP 摄像头配置
 * @brief DVP 接口摄像头的引脚配置
 *
 * DVP（Digital Video Port）是并行摄像头接口，使用 8 位数据总线传输图像数据。
 * @{
 */
#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
/**
 * @def EXAMPLE_DVP_SCCB_I2C_SCL_PIN
 * @brief DVP 摄像头 SCCB 总线 SCL 引脚（GPIO8）
 */
#define EXAMPLE_DVP_SCCB_I2C_SCL_PIN                    8
/**
 * @def EXAMPLE_DVP_SCCB_I2C_SDA_PIN
 * @brief DVP 摄像头 SCCB 总线 SDA 引脚（GPIO7）
 */
#define EXAMPLE_DVP_SCCB_I2C_SDA_PIN                    7
/**
 * @def EXAMPLE_DVP_CAM_SENSOR_RESET_PIN
 * @brief DVP 摄像头复位引脚（GPIO36）
 */
#define EXAMPLE_DVP_CAM_SENSOR_RESET_PIN                36
/**
 * @def EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN
 * @brief DVP 摄像头电源使能引脚（GPIO38）
 */
#define EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN                 38

/**
 * @defgroup ESP32_P4_FUNCTION_EV_V15_DVP_PIN_CONFIG DVP 接口引脚配置
 * @brief DVP 接口的时序和数据引脚配置
 * @{
 */
/**
 * @def EXAMPLE_DVP_XCLK_PIN
 * @brief DVP 摄像头外部时钟引脚（GPIO20）
 */
#define EXAMPLE_DVP_XCLK_PIN                            20
/**
 * @def EXAMPLE_DVP_PCLK_PIN
 * @brief DVP 像素时钟引脚（GPIO4）
 *
 * 像素时钟用于同步图像数据的采样。
 */
#define EXAMPLE_DVP_PCLK_PIN                            4
/**
 * @def EXAMPLE_DVP_VSYNC_PIN
 * @brief DVP 垂直同步引脚（GPIO37）
 *
 * 垂直同步信号表示一帧图像的开始。
 */
#define EXAMPLE_DVP_VSYNC_PIN                           37
/**
 * @def EXAMPLE_DVP_DE_PIN
 * @brief DVP 数据使能引脚（GPIO22）
 *
 * 数据使能信号表示当前像素数据有效。
 */
#define EXAMPLE_DVP_DE_PIN                              22
/**
 * @def EXAMPLE_DVP_D0_PIN ~ EXAMPLE_DVP_D7_PIN
 * @brief DVP 8 位数据总线引脚
 *
 * D0-D7 组成 8 位并行数据总线，用于传输图像像素数据。
 */
#define EXAMPLE_DVP_D0_PIN                              2
#define EXAMPLE_DVP_D1_PIN                              32
#define EXAMPLE_DVP_D2_PIN                              33
#define EXAMPLE_DVP_D3_PIN                              23
#define EXAMPLE_DVP_D4_PIN                              3
#define EXAMPLE_DVP_D5_PIN                              6
#define EXAMPLE_DVP_D6_PIN                              5
#define EXAMPLE_DVP_D7_PIN                              21
/** @} */
#endif /* CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR */
/** @} */

/**
 * @defgroup ESP32_P4_FUNCTION_EV_V15_SPI_CONFIG SPI 摄像头配置
 * @brief SPI 接口摄像头的引脚配置
 * @{
 */
#if CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR
/**
 * @def EXAMPLE_SPI_CAM_0_SCCB_I2C_SCL_PIN
 * @brief SPI 摄像头 0 SCCB 总线 SCL 引脚（GPIO8）
 */
#define EXAMPLE_SPI_CAM_0_SCCB_I2C_SCL_PIN              8
/**
 * @def EXAMPLE_SPI_CAM_0_SCCB_I2C_SDA_PIN
 * @brief SPI 摄像头 0 SCCB 总线 SDA 引脚（GPIO7）
 */
#define EXAMPLE_SPI_CAM_0_SCCB_I2C_SDA_PIN              7
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
 * @defgroup ESP32_P4_FUNCTION_EV_V15_SPI_PIN_CONFIG SPI 接口引脚配置
 * @brief SPI 接口的控制和数据引脚配置
 * @{
 */
/**
 * @def EXAMPLE_SPI_CAM_0_CS_PIN
 * @brief SPI 摄像头 0 片选引脚（GPIO37）
 */
#define EXAMPLE_SPI_CAM_0_CS_PIN                        37
/**
 * @def EXAMPLE_SPI_CAM_0_SCLK_PIN
 * @brief SPI 摄像头 0 时钟引脚（GPIO4）
 */
#define EXAMPLE_SPI_CAM_0_SCLK_PIN                      4
/**
 * @def EXAMPLE_SPI_CAM_0_DATA0_IO_PIN
 * @brief SPI 摄像头 0 双向数据引脚（GPIO21）
 */
#define EXAMPLE_SPI_CAM_0_DATA0_IO_PIN                  21
/** @} */

/**
 * @def EXAMPLE_SPI_CAM_0_XCLK_PIN
 * @brief SPI 摄像头 0 外部时钟引脚（GPIO20）
 */
#define EXAMPLE_SPI_CAM_0_XCLK_PIN                      20
#endif /* CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR */
/** @} */

/** @} */

#ifdef __cplusplus
}
#endif
