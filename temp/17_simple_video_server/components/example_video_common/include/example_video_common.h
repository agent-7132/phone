/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

/**
 * @file example_video_common.h
 * @brief 视频系统公共接口头文件
 *
 * 本文件定义了视频系统的公共 API 和配置宏，是组件对外的主要接口文件。
 * 包含以下内容：
 * 1. 摄像头接口配置宏（MIPI-CSI、DVP、SPI、USB-UVC）
 * 2. 编码器相关数据类型和 API 声明
 * 3. 视频系统初始化/反初始化 API 声明
 *
 * 使用方法：
 * @code{c}
 * #include "example_video_common.h"
 *
 * // 初始化视频系统
 * ESP_ERROR_CHECK(example_video_init());
 *
 * // 初始化编码器（非硬件JPEG输出时）
 * example_encoder_config_t config = {
 *     .width = 640,
 *     .height = 480,
 *     .pixel_format = V4L2_PIX_FMT_RGB565,
 *     .quality = 80,
 * };
 * example_encoder_handle_t encoder;
 * ESP_ERROR_CHECK(example_encoder_init(&config, &encoder));
 *
 * // 分配输出缓冲区
 * uint8_t *jpeg_buf;
 * uint32_t jpeg_buf_size;
 * ESP_ERROR_CHECK(example_encoder_alloc_output_buffer(encoder, &jpeg_buf, &jpeg_buf_size));
 *
 * // 编码处理
 * uint32_t out_size;
 * ESP_ERROR_CHECK(example_encoder_process(encoder, src_buf, src_size, jpeg_buf, jpeg_buf_size, &out_size));
 *
 * // 清理
 * example_encoder_free_output_buffer(encoder, jpeg_buf);
 * example_encoder_deinit(encoder);
 * @endcode
 */

#pragma once

#include "linux/videodev2.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "esp_video_ioctl.h"
#include "example_video_common_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup EXAMPLE_VIDEO_COMMON_CONFIG 视频系统配置宏
 * @brief 摄像头接口配置宏定义，根据 menuconfig 配置生成
 * @{
 */

/**
 * @def EXAMPLE_SCCB_I2C_PORT_INIT_BY_APP
 * @brief SCCB(I2C) 应用层初始化端口号
 *
 * 当 CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP 启用时，由应用层统一初始化 SCCB 总线，
 * 所有摄像头接口共享此总线。端口号从 menuconfig 读取。
 */
#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_SCCB_I2C_PORT_INIT_BY_APP              CONFIG_EXAMPLE_SCCB_I2C_PORT_INIT_BY_APP
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

/**
 * @defgroup EXAMPLE_MIPI_CSI_CONFIG MIPI-CSI 摄像头配置
 * @brief MIPI-CSI 接口摄像头的配置宏
 * @{
 */
#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
#define EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR              1

#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_MIPI_CSI_SCCB_I2C_PORT                  EXAMPLE_SCCB_I2C_PORT_INIT_BY_APP
#else /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
#define EXAMPLE_MIPI_CSI_SCCB_I2C_PORT                  CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

#define EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ                  CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ

#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR
#define EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR              1

#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_PORT       EXAMPLE_SCCB_I2C_PORT_INIT_BY_APP
#else /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_PORT       CONFIG_EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_PORT
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

#define EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_FREQ       CONFIG_EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_FREQ
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR */
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */
/** @} */

/**
 * @defgroup EXAMPLE_DVP_CONFIG DVP 摄像头配置
 * @brief DVP 接口摄像头的配置宏
 * @{
 */
#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
#define EXAMPLE_ENABLE_DVP_CAM_SENSOR                   1

#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_DVP_SCCB_I2C_PORT                       EXAMPLE_SCCB_I2C_PORT_INIT_BY_APP
#else /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
#define EXAMPLE_DVP_SCCB_I2C_PORT                       CONFIG_EXAMPLE_DVP_SCCB_I2C_PORT
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

#define EXAMPLE_DVP_SCCB_I2C_FREQ                       CONFIG_EXAMPLE_DVP_SCCB_I2C_FREQ

/**
 * @def EXAMPLE_DVP_XCLK_FREQ
 * @brief DVP 摄像头外部时钟频率
 *
 * 如果定义了 EXAMPLE_DVP_XCLK_PIN 且有效，使用 menuconfig 配置的频率；
 * 否则设置为 0（表示不使用外部时钟）。
 */
#if (defined(EXAMPLE_DVP_XCLK_PIN) && EXAMPLE_DVP_XCLK_PIN >= 0)
#define EXAMPLE_DVP_XCLK_FREQ                           CONFIG_EXAMPLE_DVP_XCLK_FREQ
#else
#define EXAMPLE_DVP_XCLK_FREQ                           0
#endif /* EXAMPLE_DVP_XCLK_PIN >= 0 */
#endif /* CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR */
/** @} */

/**
 * @defgroup EXAMPLE_SPI_CONFIG SPI 摄像头配置
 * @brief SPI 接口摄像头的配置宏（支持单/双摄像头）
 * @{
 */
#if CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR
#if CONFIG_EXAMPLE_SPI_CAM_0_INTERFACE_PARLIO
#define EXAMPLE_SPI_CAM_0_INTERFACE_PARLIO               1
#else
#define EXAMPLE_SPI_CAM_0_INTERFACE_PARLIO               0
#endif /* CONFIG_EXAMPLE_SPI_CAM_0_INTERFACE_PARLIO */

#define EXAMPLE_ENABLE_SPI_CAM_SENSOR                   1
#define EXAMPLE_ENABLE_SPI_CAM_0_SENSOR                 1

#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_SPI_CAM_0_SCCB_I2C_PORT                 EXAMPLE_SCCB_I2C_PORT_INIT_BY_APP
#else /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
#define EXAMPLE_SPI_CAM_0_SCCB_I2C_PORT                 CONFIG_EXAMPLE_SPI_CAM_0_SCCB_I2C_PORT
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

#define EXAMPLE_SPI_CAM_0_SCCB_I2C_FREQ                 CONFIG_EXAMPLE_SPI_CAM_0_SCCB_I2C_FREQ

/**
 * @def EXAMPLE_SPI_CAM_0_SPI_PORT
 * @brief SPI 摄像头 0 的 SPI 端口号
 */
#define EXAMPLE_SPI_CAM_0_SPI_PORT                      CONFIG_EXAMPLE_SPI_CAM_0_SPI_PORT

/**
 * @def EXAMPLE_SPI_CAM_0_XCLK_RESOURCE
 * @brief SPI 摄像头 0 的 XCLK 时钟资源类型
 *
 * 根据 menuconfig 配置选择 LEDC 或时钟路由器。
 */
#if CONFIG_EXAMPLE_SPI_CAM_0_XCLK_USE_LEDC
#define EXAMPLE_SPI_CAM_0_XCLK_RESOURCE                 ESP_CAM_SENSOR_XCLK_LEDC
#elif CONFIG_EXAMPLE_SPI_CAM_0_XCLK_USE_CLOCK_ROUTER
#define EXAMPLE_SPI_CAM_0_XCLK_RESOURCE                 ESP_CAM_SENSOR_XCLK_ESP_CLOCK_ROUTER
#endif /* CONFIG_EXAMPLE_SPI_CAM_0_XCLK_USE_LEDC */

/**
 * @def EXAMPLE_SPI_CAM_0_XCLK_FREQ
 * @brief SPI 摄像头 0 的 XCLK 时钟频率
 *
 * 如果定义了 EXAMPLE_SPI_CAM_0_XCLK_PIN 且有效，使用 menuconfig 配置的频率；
 * 否则设置为 0。
 */
#if defined(EXAMPLE_SPI_CAM_0_XCLK_PIN) && EXAMPLE_SPI_CAM_0_XCLK_PIN >= 0
#define EXAMPLE_SPI_CAM_0_XCLK_FREQ                     CONFIG_EXAMPLE_SPI_CAM_0_XCLK_FREQ
#else
#define EXAMPLE_SPI_CAM_0_XCLK_FREQ                     0
#endif /* EXAMPLE_SPI_CAM_0_XCLK_PIN >= 0 */

/**
 * @def EXAMPLE_SPI_CAM_0_XCLK_TIMER
 * @brief SPI 摄像头 0 的 XCLK LEDC 定时器编号
 */
#if CONFIG_EXAMPLE_SPI_CAM_0_XCLK_USE_LEDC
#define EXAMPLE_SPI_CAM_0_XCLK_TIMER                    CONFIG_EXAMPLE_SPI_CAM_0_XCLK_TIMER
#define EXAMPLE_SPI_CAM_0_XCLK_TIMER_CHANNEL            CONFIG_EXAMPLE_SPI_CAM_0_XCLK_TIMER_CHANNEL
#endif /* CONFIG_EXAMPLE_SPI_CAM_XCLK_USE_LEDC */

#if CONFIG_EXAMPLE_ENABLE_SPI_CAM_1_SENSOR
#define EXAMPLE_ENABLE_SPI_CAM_1_SENSOR                 1

#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_SPI_CAM_1_SCCB_I2C_PORT                 EXAMPLE_SCCB_I2C_PORT_INIT_BY_APP
#else /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
#define EXAMPLE_SPI_CAM_1_SCCB_I2C_PORT                 CONFIG_EXAMPLE_SPI_CAM_1_SCCB_I2C_PORT
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

#define EXAMPLE_SPI_CAM_1_SCCB_I2C_FREQ                 CONFIG_EXAMPLE_SPI_CAM_1_SCCB_I2C_FREQ

#define EXAMPLE_SPI_CAM_1_SPI_PORT                      CONFIG_EXAMPLE_SPI_CAM_1_SPI_PORT

#if CONFIG_EXAMPLE_SPI_CAM_1_XCLK_USE_LEDC
#define EXAMPLE_SPI_CAM_1_XCLK_RESOURCE                 ESP_CAM_SENSOR_XCLK_LEDC
#elif CONFIG_EXAMPLE_SPI_CAM_1_XCLK_USE_CLOCK_ROUTER
#define EXAMPLE_SPI_CAM_1_XCLK_RESOURCE                 ESP_CAM_SENSOR_XCLK_ESP_CLOCK_ROUTER
#endif /* CONFIG_EXAMPLE_SPI_CAM_1_XCLK_USE_LEDC */

#if defined(EXAMPLE_SPI_CAM_1_XCLK_PIN) && EXAMPLE_SPI_CAM_1_XCLK_PIN >= 0
#define EXAMPLE_SPI_CAM_1_XCLK_FREQ                     CONFIG_EXAMPLE_SPI_CAM_1_XCLK_FREQ
#else
#define EXAMPLE_SPI_CAM_1_XCLK_FREQ                     0
#endif /* EXAMPLE_SPI_CAM_1_XCLK_PIN >= 0 */

#if CONFIG_EXAMPLE_SPI_CAM_1_XCLK_USE_LEDC
#define EXAMPLE_SPI_CAM_1_XCLK_TIMER                    CONFIG_EXAMPLE_SPI_CAM_1_XCLK_TIMER
#define EXAMPLE_SPI_CAM_1_XCLK_TIMER_CHANNEL            CONFIG_EXAMPLE_SPI_CAM_1_XCLK_TIMER_CHANNEL
#endif /* CONFIG_EXAMPLE_SPI_CAM_1_XCLK_USE_LEDC */
#endif /* CONFIG_EXAMPLE_ENABLE_SPI_CAM_1_SENSOR */
#endif /* CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR */
/** @} */

/**
 * @defgroup EXAMPLE_USB_UVC_CONFIG USB-UVC 摄像头配置
 * @brief USB UVC 接口摄像头的配置宏
 * @{
 */
#if CONFIG_EXAMPLE_ENABLE_USB_UVC_CAM_SENSOR
#define EXAMPLE_ENABLE_USB_UVC_CAM_SENSOR               1
#endif

/**
 * @def EXAMPLE_CAM_DEV_PATH
 * @brief 默认摄像头设备路径
 *
 * 根据启用的摄像头接口类型，选择对应的设备路径：
 * - MIPI-CSI: ESP_VIDEO_MIPI_CSI_DEVICE_NAME
 * - DVP: ESP_VIDEO_DVP_DEVICE_NAME
 * - SPI: ESP_VIDEO_SPI_DEVICE_NAME
 * - USB-UVC: ESP_VIDEO_USB_UVC_DEVICE_NAME(0)
 *
 * @note 优先级：MIPI-CSI > DVP > SPI > USB-UVC
 */
#if EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
#define EXAMPLE_CAM_DEV_PATH                            ESP_VIDEO_MIPI_CSI_DEVICE_NAME
#elif EXAMPLE_ENABLE_DVP_CAM_SENSOR
#define EXAMPLE_CAM_DEV_PATH                            ESP_VIDEO_DVP_DEVICE_NAME
#elif EXAMPLE_ENABLE_SPI_CAM_SENSOR
#define EXAMPLE_CAM_DEV_PATH                            ESP_VIDEO_SPI_DEVICE_NAME
#elif EXAMPLE_ENABLE_USB_UVC_CAM_SENSOR
#define EXAMPLE_CAM_DEV_PATH                            ESP_VIDEO_USB_UVC_DEVICE_NAME(0)
#else
#define EXAMPLE_CAM_DEV_PATH                            ESP_VIDEO_SPI_DEVICE_NAME
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */
/** @} */

/** @} */

/**
 * @defgroup EXAMPLE_ENCODER_API JPEG 编码器 API
 * @brief JPEG 编码器的统一接口，支持硬件和软件编码
 * @{
 */

/**
 * @typedef example_encoder_handle_t
 * @brief JPEG 编码器句柄类型
 *
 * 不透明指针，用于标识编码器实例，由 example_encoder_init() 创建，
 * 由 example_encoder_deinit() 销毁。
 */
typedef void *example_encoder_handle_t;

/**
 * @struct example_encoder_config
 * @brief JPEG 编码器配置结构
 *
 * 包含编码器初始化所需的所有参数。
 */
typedef struct example_encoder_config {
    uint32_t width;             /**< 图像宽度（像素） */
    uint32_t height;            /**< 图像高度（像素） */
    uint32_t pixel_format;      /**< 输入像素格式，使用 V4L2 格式定义（如 V4L2_PIX_FMT_RGB565） */
    uint8_t quality;            /**< JPEG 编码质量（1-100，值越大质量越高） */
} example_encoder_config_t;

/**
 * @brief 初始化视频系统
 *
 * 初始化视频驱动和摄像头传感器，支持 MIPI-CSI、DVP、SPI、USB-UVC 等多种接口。
 * 此函数应在设备启动后尽早调用，特别是对于需要 XCLK 时钟的摄像头设备。
 *
 * @return ESP_OK 成功，其他错误码表示失败
 * @see example_video_deinit()
 */
esp_err_t example_video_init(void);

/**
 * @brief 反初始化视频系统
 *
 * 释放视频驱动和摄像头相关资源，按逆序释放初始化时分配的资源。
 *
 * @return ESP_OK 成功，其他错误码表示失败
 * @see example_video_init()
 */
esp_err_t example_video_deinit(void);

/**
 * @brief 初始化 JPEG 编码器
 *
 * 根据配置创建编码器实例，支持硬件编码（ESP32-P4 JPEG 引擎）和软件编码。
 *
 * @param config 编码器配置，包含图像尺寸、像素格式和质量
 * @param ret_handle 输出参数，返回编码器句柄
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数无效，ESP_ERR_NO_MEM 内存不足，
 *         ESP_ERR_NOT_SUPPORTED 不支持的像素格式
 * @see example_encoder_deinit()
 */
esp_err_t example_encoder_init(example_encoder_config_t *config, example_encoder_handle_t *ret_handle);

/**
 * @brief 分配编码器输出缓冲区
 *
 * 根据编码器配置分配适当大小的 JPEG 输出缓冲区。缓冲区大小取决于输入图像尺寸和质量设置。
 *
 * @param handle 编码器句柄
 * @param buf 输出参数，返回分配的缓冲区指针
 * @param size 输出参数，返回缓冲区大小（字节）
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数无效，ESP_ERR_NO_MEM 内存不足
 * @see example_encoder_free_output_buffer()
 */
esp_err_t example_encoder_alloc_output_buffer(example_encoder_handle_t handle, uint8_t **buf, uint32_t *size);

/**
 * @brief 释放编码器输出缓冲区
 *
 * 释放之前通过 example_encoder_alloc_output_buffer() 分配的缓冲区。
 *
 * @param handle 编码器句柄
 * @param buf 要释放的缓冲区指针
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数无效
 * @see example_encoder_alloc_output_buffer()
 */
esp_err_t example_encoder_free_output_buffer(example_encoder_handle_t handle, uint8_t *buf);

/**
 * @brief 执行 JPEG 编码
 *
 * 将原始图像数据编码为 JPEG 格式。调用前需确保编码器已初始化且输出缓冲区已分配。
 *
 * @param handle 编码器句柄
 * @param src_buf 源图像数据缓冲区
 * @param src_size 源数据大小（字节）
 * @param dst_buf JPEG 输出缓冲区
 * @param dst_size 输出缓冲区大小（字节）
 * @param dst_size_out 输出参数，返回实际编码后的数据大小（字节）
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数无效，其他错误码表示编码失败
 */
esp_err_t example_encoder_process(example_encoder_handle_t handle, uint8_t *src_buf, uint32_t src_size, uint8_t *dst_buf, uint32_t dst_size, uint32_t *dst_size_out);

/**
 * @brief 设置 JPEG 编码质量
 *
 * 动态调整编码器的 JPEG 质量设置，无需重新初始化编码器。
 *
 * @param handle 编码器句柄
 * @param quality JPEG 质量值（1-100）
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数无效
 */
esp_err_t example_encoder_set_jpeg_quality(example_encoder_handle_t handle, uint8_t quality);

/**
 * @brief 反初始化编码器
 *
 * 释放编码器相关资源，包括编码器实例和内部状态。
 *
 * @param handle 编码器句柄
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数无效
 * @see example_encoder_init()
 */
esp_err_t example_encoder_deinit(example_encoder_handle_t handle);

/** @} */

#ifdef __cplusplus
}
#endif
