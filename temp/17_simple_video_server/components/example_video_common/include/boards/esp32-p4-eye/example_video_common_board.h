/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

/**
 * @file example_video_common_board.h
 * @brief ESP32-P4-EYE 开发板硬件配置文件
 *
 * ESP32-P4-EYE 是 Espressif 官方推出的 AI 视觉开发板，搭载 ESP32-P4 芯片，
 * 集成了 MIPI-CSI 摄像头接口。本文件定义了该开发板的硬件引脚映射和配置参数。
 *
 * @note ESP32-P4-EYE 默认仅支持 MIPI-CSI 摄像头接口，DVP 和 SPI 接口不支持。
 */

#pragma once

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ESP32_P4_EYE_CONFIG ESP32-P4-EYE 开发板配置
 * @brief ESP32-P4-EYE 开发板的硬件引脚和接口配置
 * @{
 */

/**
 * @defgroup ESP32_P4_EYE_SCCB_CONFIG SCCB(I2C) 总线配置
 * @brief SCCB(I2C) 总线的引脚配置
 *
 * SCCB 是摄像头控制总线，用于与摄像头传感器进行通信。
 * ESP32-P4-EYE 使用 GPIO13(SCL) 和 GPIO14(SDA)。
 * @{
 */
#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
/**
 * @def EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP
 * @brief 应用层统一初始化 SCCB 总线时的 SCL 引脚
 */
#define EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP            13
/**
 * @def EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP
 * @brief 应用层统一初始化 SCCB 总线时的 SDA 引脚
 */
#define EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP            14
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
/** @} */

/**
 * @defgroup ESP32_P4_EYE_MIPI_CSI_CONFIG MIPI-CSI 摄像头配置
 * @brief MIPI-CSI 接口摄像头的引脚配置
 *
 * ESP32-P4-EYE 板载 OV5640 摄像头，通过 MIPI-CSI 接口连接。
 * @{
 */
#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
/**
 * @def EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN
 * @brief MIPI-CSI 摄像头 SCCB 总线 SCL 引脚
 */
#define EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN               13
/**
 * @def EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN
 * @brief MIPI-CSI 摄像头 SCCB 总线 SDA 引脚
 */
#define EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN               14

/**
 * @def EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN
 * @brief MIPI-CSI 摄像头复位引脚
 *
 * GPIO26 用于控制摄像头传感器的复位信号，低电平有效。
 */
#define EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN           26
/**
 * @def EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN
 * @brief MIPI-CSI 摄像头电源使能引脚
 *
 * GPIO12 用于控制摄像头传感器的电源，高电平有效。
 */
#define EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN            12
/**
 * @def EXAMPLE_MIPI_CSI_XCLK_PIN
 * @brief MIPI-CSI 摄像头外部时钟引脚
 *
 * GPIO11 用于提供摄像头所需的外部时钟信号。
 */
#define EXAMPLE_MIPI_CSI_XCLK_PIN                       11
/**
 * @def EXAMPLE_MIPI_CSI_XCLK_FREQ
 * @brief MIPI-CSI 摄像头外部时钟频率
 *
 * 设置为 24MHz，满足 OV5640 摄像头的时钟需求。
 */
#define EXAMPLE_MIPI_CSI_XCLK_FREQ                      24000000

#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR
/**
 * @brief ESP32-P4-EYE 默认不支持摄像头电机功能
 *
 * ESP32-P4-EYE 开发板没有集成摄像头电机驱动电路，
 * 如果启用此配置会导致编译错误。
 */
#error "MIPI-CSI camera motor is not supported on ESP32-P4-EYE by default"
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR */
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */
/** @} */

/**
 * @defgroup ESP32_P4_EYE_DVP_CONFIG DVP 摄像头配置
 * @brief DVP 接口摄像头配置（不支持）
 * @{
 */
#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
/**
 * @brief ESP32-P4-EYE 默认不支持 DVP 摄像头接口
 *
 * ESP32-P4-EYE 开发板的硬件设计仅支持 MIPI-CSI 接口，
 * 如果启用此配置会导致编译错误。
 */
#error "DVP interface camera sensor is not supported on ESP32-P4-EYE by default"
#endif /* CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR */
/** @} */

/**
 * @defgroup ESP32_P4_EYE_SPI_CONFIG SPI 摄像头配置
 * @brief SPI 接口摄像头配置（不支持）
 * @{
 */
#if CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR
/**
 * @brief ESP32-P4-EYE 默认不支持 SPI 摄像头接口
 *
 * ESP32-P4-EYE 开发板的硬件设计仅支持 MIPI-CSI 接口，
 * 如果启用此配置会导致编译错误。
 */
#error "SPI interface camera sensor is not supported on ESP32-P4-EYE by default"
#endif /* CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR */
/** @} */

/** @} */

#ifdef __cplusplus
}
#endif
