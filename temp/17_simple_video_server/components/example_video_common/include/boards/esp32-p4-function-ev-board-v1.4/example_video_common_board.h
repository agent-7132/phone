/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

/**
 * @file example_video_common_board.h
 * @brief ESP32-P4-Function-EV-Board V1.4 开发板硬件配置文件
 *
 * ESP32-P4-Function-EV-Board V1.4 是 ESP32-P4-Function-EV-Board 的早期版本，
 * 默认仅支持 MIPI-CSI 摄像头接口。本文件定义了该开发板的硬件引脚映射。
 */

#pragma once

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ESP32_P4_FUNCTION_EV_V14_CONFIG ESP32-P4-Function-EV-Board V1.4 配置
 * @brief ESP32-P4-Function-EV-Board V1.4 开发板的硬件引脚和接口配置
 * @{
 */

/**
 * @defgroup ESP32_P4_FUNCTION_EV_V14_SCCB_CONFIG SCCB(I2C) 总线配置
 * @brief SCCB(I2C) 总线的引脚配置
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
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
/** @} */

/**
 * @defgroup ESP32_P4_FUNCTION_EV_V14_MIPI_CSI_CONFIG MIPI-CSI 摄像头配置
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
 * @defgroup ESP32_P4_FUNCTION_EV_V14_DVP_CONFIG DVP 摄像头配置
 * @brief DVP 接口摄像头配置（不支持）
 * @{
 */
#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
/**
 * @brief ESP32-P4-Function-EV-Board V1.4 默认不支持 DVP 摄像头接口
 *
 * V1.4 版本的硬件设计仅支持 MIPI-CSI 接口，DVP 接口在 V1.5 版本中才支持。
 * 如果启用此配置会导致编译错误。
 */
#error "DVP interface camera sensor is not supported on ESP32-P4-Function-EV-Board V1.4 by default"
#endif /* CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR */
/** @} */

/**
 * @defgroup ESP32_P4_FUNCTION_EV_V14_SPI_CONFIG SPI 摄像头配置
 * @brief SPI 接口摄像头配置（不支持）
 * @{
 */
#if CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR
/**
 * @brief ESP32-P4-Function-EV-Board V1.4 默认不支持 SPI 摄像头接口
 *
 * V1.4 版本的硬件设计仅支持 MIPI-CSI 接口，SPI 接口在 V1.5 版本中才支持。
 * 如果启用此配置会导致编译错误。
 */
#error "SPI interface camera sensor is not supported on ESP32-P4-Function-EV-Board V1.4 by default"
#endif /* CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR */
/** @} */

/** @} */

#ifdef __cplusplus
}
#endif
