/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file app_video.h
 * @brief 视频应用模块头文件
 * 
 * 本模块提供基于V4L2标准的视频采集接口，支持CSI摄像头的视频流处理。
 * 
 * 核心功能：
 * - 视频设备初始化和配置
 * - 帧缓冲区管理（MMAP/USERPTR模式）
 * - 视频流控制（开始/停止/重启）
 * - 帧处理回调机制
 * 
 * 使用流程：
 * 1. 调用 app_video_main() 初始化视频子系统
 * 2. 调用 app_video_open() 打开视频设备
 * 3. 调用 app_video_set_bufs() 设置帧缓冲区
 * 4. 调用 app_video_register_frame_operation_cb() 注册帧处理回调
 * 5. 调用 app_video_stream_task_start() 启动视频流任务
 * 6. 调用 app_video_stream_task_stop() 停止视频流任务
 */
#ifndef APP_VIDEO_H
#define APP_VIDEO_H

#include "esp_err.h"
#include "linux/videodev2.h"
#include "esp_video_device.h"
#include "bsp/esp-bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 视频像素格式枚举
 * 
 * 定义支持的视频像素格式，与V4L2标准格式对应。
 * 
 * 格式说明：
 * - RAW8/RAW10: 原始拜尔格式，未经ISP处理的原始图像数据
 * - GREY: 灰度格式，每像素8位
 * - RGB565: 16位RGB格式，R占5位，G占6位，B占5位
 * - RGB888: 24位RGB格式，每通道8位
 * - YUV422: YUV422平面格式，亮度和色度分离
 * - YUV420: YUV420平面格式，常用于视频编码
 */
typedef enum {
    APP_VIDEO_FMT_RAW8 = V4L2_PIX_FMT_SBGGR8,   /**< RAW8拜尔格式（SBGGR排列） */
    APP_VIDEO_FMT_RAW10 = V4L2_PIX_FMT_SBGGR10, /**< RAW10拜尔格式（SBGGR排列） */
    APP_VIDEO_FMT_GREY = V4L2_PIX_FMT_GREY,     /**< 灰度格式，每像素8位 */
    APP_VIDEO_FMT_RGB565 = V4L2_PIX_FMT_RGB565, /**< 16位RGB格式，R5G6B5 */
    APP_VIDEO_FMT_RGB888 = V4L2_PIX_FMT_RGB24,  /**< 24位RGB格式，R8G8B8 */
    APP_VIDEO_FMT_YUV422 = V4L2_PIX_FMT_YUV422P, /**< YUV422平面格式 */
    APP_VIDEO_FMT_YUV420 = V4L2_PIX_FMT_YUV420,   /**< YUV420平面格式 */
} video_fmt_t;

/**
 * @brief 当前使用的视频像素格式
 * 
 * 根据BSP配置自动选择RGB565或RGB888格式：
 * - CONFIG_BSP_LCD_COLOR_FORMAT_RGB565: 使用RGB565格式（每像素2字节）
 * - CONFIG_BSP_LCD_COLOR_FORMAT_RGB888: 使用RGB888格式（每像素3字节）
 * 
 * 该宏必须在配置中明确设置，否则编译会失败。
 */
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB565
#define APP_VIDEO_FMT              (APP_VIDEO_FMT_RGB565)
#elif CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
#define APP_VIDEO_FMT              (APP_VIDEO_FMT_RGB888)
#endif

/**
 * @brief 视频帧处理回调函数类型
 * 
 * 当视频设备接收到一帧新数据时，会调用该回调函数。
 * 用户需要实现该回调函数来处理视频帧数据（如显示、编码、存储等）。
 * 
 * @param camera_buf 帧缓冲区指针，包含最新采集的图像数据
 * @param camera_buf_index 缓冲区索引（0或1，用于双缓冲切换）
 * @param camera_buf_hes 帧水平分辨率（像素）
 * @param camera_buf_ves 帧垂直分辨率（像素）
 * @param camera_buf_len 帧缓冲区长度（字节）
 * @param user_data 用户自定义数据，由app_video_stream_task_start()传入
 */
typedef void (*app_video_frame_operation_cb_t)(
    uint8_t *camera_buf,      /**< 帧缓冲区指针 */
    uint8_t camera_buf_index, /**< 缓冲区索引 */
    uint32_t camera_buf_hes,  /**< 水平分辨率 */
    uint32_t camera_buf_ves,  /**< 垂直分辨率 */
    size_t camera_buf_len,    /**< 缓冲区长度 */
    void *user_data           /**< 用户自定义数据 */
);

/**
 * @brief 初始化视频摄像头子系统
 * 
 * 配置CSI摄像头的SCCB（I2C兼容）通信参数，然后初始化视频设备。
 * SCCB是摄像头控制总线，与I2C协议兼容但时序略有差异。
 * 
 * @param i2c_bus_handle I2C总线句柄，用于SCCB通信（可为NULL）
 * @return ESP_OK 初始化成功，其他错误码表示失败
 */
esp_err_t app_video_main(i2c_master_bus_handle_t i2c_bus_handle);

/**
 * @brief 打开视频采集设备并配置格式
 * 
 * 按照V4L2标准流程初始化视频采集设备：
 * 1. 打开视频设备文件
 * 2. 查询设备能力
 * 3. 获取当前格式并保存分辨率
 * 4. 设置所需的像素格式
 * 5. 设置垂直和水平翻转
 * 
 * @param dev 视频设备路径（如"/dev/video0"）
 * @param init_fmt 初始像素格式
 * @return >=0 视频设备文件描述符，-1表示打开失败
 */
int app_video_open(char *dev, video_fmt_t init_fmt);

/**
 * @brief 设置视频采集缓冲区
 * 
 * 配置视频设备使用指定数量的帧缓冲区，支持两种内存模式：
 * - MMAP模式：内核分配缓冲区，通过mmap映射访问
 * - USERPTR模式：用户空间分配缓冲区，传递指针给内核
 * 
 * @param video_fd 视频设备文件描述符
 * @param fb_num 帧缓冲区数量（2~3个）
 * @param fb 用户提供的帧缓冲区数组（USERPTR模式），NULL表示MMAP模式
 * @return ESP_OK 设置成功，ESP_FAIL表示失败（设备已关闭）
 */
esp_err_t app_video_set_bufs(int video_fd, uint32_t fb_num, const void **fb);

/**
 * @brief 获取视频采集缓冲区指针
 * 
 * 将已配置的帧缓冲区指针填充到用户提供的数组中，用于MMAP模式获取缓冲区。
 * 
 * @param fb_num 要获取的缓冲区数量
 * @param fb 输出参数，缓冲区指针数组
 * @return ESP_OK 获取成功，ESP_FAIL表示失败
 */
esp_err_t app_video_get_bufs(int fb_num, void **fb);

/**
 * @brief 获取视频帧缓冲区大小
 * 
 * 根据摄像头分辨率和像素格式计算单个帧缓冲区的大小。
 * 
 * @return 单个帧缓冲区大小（字节）
 */
uint32_t app_video_get_buf_size(void);

/**
 * @brief 启动视频流任务
 * 
 * 启动视频流并创建FreeRTOS任务持续处理视频帧。
 * 
 * @param video_fd 视频设备文件描述符
 * @param core_id 任务运行的CPU核心ID（0或1）
 * @param user_data 用户自定义数据，透传给帧处理回调
 * @return ESP_OK 启动成功，ESP_FAIL表示失败（任务创建失败）
 */
esp_err_t app_video_stream_task_start(int video_fd, int core_id, void *user_data);

/**
 * @brief 重启视频流任务
 * 
 * 重新设置缓冲区并重启视频流任务，用于视频流异常后恢复。
 * 
 * @param video_fd 视频设备文件描述符
 * @return ESP_OK 重启成功，ESP_FAIL表示失败
 */
esp_err_t app_video_stream_task_restart(int video_fd);

/**
 * @brief 停止视频流任务
 * 
 * 设置任务删除标志，视频流任务会在下一次循环中检测到标志并自行停止。
 * 注意：该函数不会立即停止任务。
 * 
 * @param video_fd 视频设备文件描述符（保留接口一致性）
 * @return ESP_OK 停止请求已发送
 */
esp_err_t app_video_stream_task_stop(int video_fd);

/**
 * @brief 注册帧处理回调函数
 * 
 * 设置用户定义的帧处理回调函数，当视频设备接收到一帧新数据时调用。
 * 
 * @param operation_cb 帧处理回调函数指针
 * @return ESP_OK 注册成功
 */
esp_err_t app_video_register_frame_operation_cb(app_video_frame_operation_cb_t operation_cb);

#ifdef __cplusplus
}
#endif
#endif
