/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * @file main.c
 * @brief 摄像头视频LCD显示示例主文件
 * 
 * 本示例展示了如何将摄像头采集的视频实时显示到LCD屏幕上：
 * 1. 初始化BSP显示驱动
 * 2. 初始化视频子系统（CSI摄像头）
 * 3. 分配帧缓冲区（使用SPIRAM）
 * 4. 使用PPA硬件加速器进行图像缩放和格式转换
 * 5. 通过dummy draw方式将视频帧显示到LCD
 * 
 * 主要使用组件：
 * - esp_video: 视频设备驱动
 * - esp_lcd: LCD显示驱动
 * - ppa: 硬件图像处理器（缩放、旋转、镜像）
 */

#include "esp_err.h"
#include "esp_log.h"
#include "esp_video_init.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_private/esp_cache_private.h"
#include "esp_timer.h"
#include "driver/ppa.h"
#include "app_video.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"

#include "bsp/display.h"
#include "bsp/esp-bsp.h"

#include "lvgl.h"
#include "lv_demos.h"

/**
 * @brief 向上对齐宏
 * 
 * 将数值向上对齐到指定的对齐边界。常用于内存地址对齐、缓冲区大小对齐等场景。
 * 
 * @param num 要对齐的数值
 * @param align 对齐边界（必须是2的幂次）
 * @return 对齐后的数值
 */
#define ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))

/**
 * @brief 计算PPA输入偏移量
 * 
 * 根据源图像和目标图像的尺寸，计算图像居中显示时的裁剪偏移量。
 * 如果源图像小于目标图像，则偏移量为0。
 * 
 * @param src_w 源图像宽度
 * @param src_h 源图像高度
 * @param dst_w 目标图像宽度
 * @param dst_h 目标图像高度
 * @param offset_x 输出参数，水平偏移量
 * @param offset_y 输出参数，垂直偏移量
 */
static void calc_ppa_input_offset(uint32_t src_w, uint32_t src_h,
                                  uint32_t dst_w, uint32_t dst_h,
                                  uint32_t *offset_x, uint32_t *offset_y)
{
    *offset_x = (src_w > dst_w) ? (src_w - dst_w) / 2 : 0;
    *offset_y = (src_h > dst_h) ? (src_h - dst_h) / 2 : 0;

    if ((*offset_x + dst_w) > src_w)
    {
        *offset_x = 0;
    }
    if ((*offset_y + dst_h) > src_h)
    {
        *offset_y = 0;
    }
}

/**
 * @brief 摄像头视频帧处理回调函数
 * 
 * 当接收到摄像头视频帧时，该函数被调用：
 * 1. 计算图像裁剪偏移量
 * 2. 配置PPA参数（输入输出缓冲区、尺寸、颜色模式）
 * 3. 调用PPA进行图像缩放和格式转换
 * 4. 使用dummy draw方式将处理后的图像显示到LCD
 * 
 * @param camera_buf 摄像头帧缓冲区指针
 * @param camera_buf_index 帧缓冲区索引（0或1）
 * @param camera_buf_hes 摄像头帧水平分辨率
 * @param camera_buf_ves 摄像头帧垂直分辨率
 * @param camera_buf_len 帧缓冲区长度
 * @param user_data 用户数据（未使用）
 */
static void camera_video_frame_operation(
    uint8_t *camera_buf,
    uint8_t camera_buf_index,
    uint32_t camera_buf_hes,
    uint32_t camera_buf_ves,
    size_t camera_buf_len,
    void *user_data);

/**
 * @brief 日志标签
 * 
 * 用于ESP_LOG系列宏的日志输出标识，便于在日志中区分不同模块。
 */
static const char *TAG = "app_main";

/**
 * @brief LCD显示面板句柄
 * 
 * 用于控制LCD面板的底层操作，如设置显示区域、写入像素数据等。
 * 通过bsp_display_get_panel_handle()获取。
 */
static esp_lcd_panel_handle_t display_panel;

/**
 * @brief PPA（Pixel Processing Accelerator）客户端句柄
 * 
 * PPA是ESP32-P4内置的硬件图像处理器，支持图像缩放、旋转、镜像等操作。
 * 这里使用SRM（Scale Rotate Mirror）操作模式。
 */
static ppa_client_handle_t ppa_srm_handle = NULL;

/**
 * @brief 数据缓存行大小
 * 
 * SPIRAM的数据缓存行对齐大小，用于确保帧缓冲区地址满足缓存对齐要求。
 * 通过esp_cache_get_alignment()获取。
 */
static size_t data_cache_line_size = 0;

/**
 * @brief LCD帧缓冲区数组
 * 
 * 存储LCD显示面板的多个帧缓冲区指针，数量由CONFIG_BSP_LCD_DPI_BUFFER_NUMS配置。
 * 使用多缓冲机制可以减少显示撕裂，提高画面流畅度。
 */
static void *lcd_buffer[CONFIG_BSP_LCD_DPI_BUFFER_NUMS];

/**
 * @brief LVGL显示驱动句柄
 * 
 * LVGL图形库的显示接口句柄，用于LVGL与底层LCD驱动的交互。
 * 通过bsp_display_start()获取。
 */
static lv_display_t *disp;

/**
 * @brief I2C总线句柄
 * 
 * 用于CSI摄像头SCCB（I2C兼容）通信的总线句柄，通过bsp_i2c_get_handle()获取。
 */
i2c_master_bus_handle_t i2c_bus_;

/**
 * @brief Dummy draw模式使能标志
 * 
 * 当设置为true时，使用LVGL的dummy draw功能直接将处理后的图像数据写入LCD帧缓冲区，
 * 绕过LVGL的标准渲染流程，提高视频显示帧率。
 */
static bool dummy_draw_enabled = true;

/**
 * @brief Dummy draw模式延迟标志
 * 
 * 用于控制dummy draw的执行时机，当设置为true时跳过当前帧的dummy draw操作。
 * 可用于帧率控制或同步场景。
 */
static bool dummy_mode_delay_flag = false;

/**
 * @brief ESP-IDF应用程序入口函数
 * 
 * 主函数负责初始化视频显示系统的完整流程：
 * 
 * 初始化流程详解：
 * 1. 启动BSP显示驱动，获取LVGL显示句柄并开启背光
 * 2. 启用dummy draw模式，绕过LVGL标准渲染流程，直接操作帧缓冲区
 * 3. 注册PPA客户端，使用SRM（Scale Rotate Mirror）模式进行硬件加速
 * 4. 获取SPIRAM的数据缓存行对齐大小，确保内存分配满足缓存对齐要求
 * 5. 获取BSP提供的I2C总线句柄，用于CSI摄像头的SCCB通信
 * 6. 初始化视频子系统，配置CSI摄像头的SCCB参数
 * 7. 打开视频设备，设置像素格式和翻转控制
 * 8. 获取LCD面板的3个帧缓冲区指针，用于双/三缓冲显示
 * 9. 分配2个摄像头帧缓冲区（使用SPIRAM），支持双缓冲采集
 * 10. 注册帧处理回调函数，处理每帧视频数据
 * 11. 启动视频流任务，开始实时视频采集和显示
 * 
 * 数据流说明：
 * 摄像头采集 → 帧缓冲区 → PPA硬件处理（缩放/裁剪）→ LCD帧缓冲区 → 显示
 * 
 * @return void 无返回值
 */
void app_main(void)
{
    /* 启动BSP显示驱动，返回LVGL显示句柄 */
    disp = bsp_display_start();
    /* 开启LCD背光 */
    bsp_display_backlight_on();

    /* 启用dummy draw模式，允许直接写入帧缓冲区 */
    ESP_ERROR_CHECK(esp_lv_adapter_set_dummy_draw(disp, dummy_draw_enabled));

    /* 获取LCD面板句柄，用于后续帧缓冲区操作 */
    display_panel = bsp_display_get_panel_handle();

    /* 配置PPA客户端，使用SRM（缩放旋转镜像）操作模式 */
    ppa_client_config_t ppa_srm_config = {
        .oper_type = PPA_OPERATION_SRM,
    };
    ESP_ERROR_CHECK(ppa_register_client(&ppa_srm_config, &ppa_srm_handle));

    /* 获取SPIRAM的数据缓存行对齐大小 */
    ESP_ERROR_CHECK(esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &data_cache_line_size));

    /* 获取I2C总线句柄，用于摄像头配置 */
    i2c_bus_ = bsp_i2c_get_handle();

    /* 初始化视频子系统 */
    esp_err_t ret = app_video_main(i2c_bus_);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "video main init failed with err=0x%x", ret);
        return;
    }

    /* 打开视频设备，设置像素格式为APP_VIDEO_FMT（RGB565或RGB888） */
    int video_cam_fd0 = app_video_open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, APP_VIDEO_FMT);
    if (video_cam_fd0 < 0)
    {
        ESP_LOGE(TAG, "video cam open failed");
        return;
    }

    /* 获取LCD面板的3个帧缓冲区指针 */
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(
        display_panel, 3,
        &lcd_buffer[0], &lcd_buffer[1], &lcd_buffer[2]));

    /* 使用用户定义的摄像头帧缓冲区（双缓冲） */
    ESP_LOGI(TAG, "Using user defined buffer");
    void *camera_buf[2];
    for (int i = 0; i < 2; i++)
    {
        /* 在SPIRAM中分配对齐的帧缓冲区 */
        camera_buf[i] = heap_caps_aligned_calloc(
            data_cache_line_size,
            1,
            app_video_get_buf_size(),
            MALLOC_CAP_SPIRAM);
    }
    /* 将帧缓冲区注册到视频设备 */
    ESP_ERROR_CHECK(app_video_set_bufs(video_cam_fd0, 2, (void *)camera_buf));

    /* 注册帧处理回调函数 */
    ESP_ERROR_CHECK(app_video_register_frame_operation_cb(camera_video_frame_operation));

    /* 启动视频流任务（运行在CPU核心0） */
    ESP_ERROR_CHECK(app_video_stream_task_start(video_cam_fd0, 0, NULL));

}

/**
 * @brief 摄像头视频帧处理回调函数
 * 
 * 当视频设备接收到一帧新的摄像头数据时，该回调函数被调用。
 * 主要功能包括：
 * 1. 检查dummy draw模式状态，决定是否处理当前帧
 * 2. 获取LCD显示分辨率，验证显示参数有效性
 * 3. 计算图像居中显示所需的裁剪偏移量
 * 4. 配置PPA硬件加速器参数（输入输出缓冲区、尺寸、颜色模式）
 * 5. 调用PPA执行图像缩放和裁剪操作
 * 6. 使用dummy draw方式将处理后的图像显示到LCD
 * 
 * 图像处理策略：
 * - 当摄像头分辨率大于LCD分辨率时：裁剪摄像头图像中心区域，保持1:1缩放
 * - 当摄像头分辨率小于LCD分辨率时：显示完整摄像头图像，不进行缩放
 * 
 * @param camera_buf 摄像头帧缓冲区指针（已包含最新采集的图像数据）
 * @param camera_buf_index 帧缓冲区索引（0或1，用于双缓冲切换）
 * @param camera_buf_hes 摄像头帧水平分辨率（像素）
 * @param camera_buf_ves 摄像头帧垂直分辨率（像素）
 * @param camera_buf_len 帧缓冲区长度（字节）
 * @param user_data 用户数据（未使用）
 */
static void camera_video_frame_operation(
    uint8_t *camera_buf,
    uint8_t camera_buf_index,
    uint32_t camera_buf_hes,
    uint32_t camera_buf_ves,
    size_t camera_buf_len,
    void *user_data)
{
    /* 检查dummy draw模式状态，如果禁用或延迟标志已设置，则跳过当前帧处理 */
    if (!dummy_draw_enabled || dummy_mode_delay_flag)
    {
        return;
    }

    /* 获取LCD显示面板的分辨率 */
    const uint32_t display_width = BSP_LCD_H_RES;
    const uint32_t display_height = BSP_LCD_V_RES;

    /* 验证显示高度有效性，防止后续除零错误 */
    if (display_height == 0)
    {
        ESP_LOGE(TAG, "Display height is zero!");
        return;
    }

    /* 计算图像居中显示时的裁剪偏移量 */
    uint32_t in_offset_x = 0;
    uint32_t in_offset_y = 0;
    calc_ppa_input_offset(camera_buf_hes, camera_buf_ves,
                          display_width, display_height,
                          &in_offset_x, &in_offset_y);

    /* 计算PPA输入块尺寸：取摄像头分辨率和显示分辨率的较小值 */
    uint32_t in_block_w = (camera_buf_hes > display_width) ? display_width : camera_buf_hes;
    uint32_t in_block_h = (camera_buf_ves > display_height) ? display_height : camera_buf_ves;

    /* 配置PPA SRM（缩放旋转镜像）操作参数 */
    ppa_srm_oper_config_t srm_config = {
        /* 输入配置 */
        .in.buffer = camera_buf,                  /**< 输入图像缓冲区指针 */
        .in.pic_w = camera_buf_hes,               /**< 输入图像总宽度 */
        .in.pic_h = camera_buf_ves,               /**< 输入图像总高度 */
        .in.block_w = in_block_w,                 /**< 处理区域宽度 */
        .in.block_h = in_block_h,                 /**< 处理区域高度 */
        .in.block_offset_x = in_offset_x,         /**< 水平偏移量（从图像中心开始） */
        .in.block_offset_y = in_offset_y,         /**< 垂直偏移量（从图像中心开始） */
        .in.srm_cm = APP_VIDEO_FMT == APP_VIDEO_FMT_RGB565 ? PPA_SRM_COLOR_MODE_RGB565 : PPA_SRM_COLOR_MODE_RGB888,

        /* 输出配置 */
        .out.buffer = lcd_buffer[camera_buf_index], /**< 输出缓冲区（LCD帧缓冲区） */
        .out.buffer_size = ALIGN_UP(display_width * display_height *
                                        (APP_VIDEO_FMT == APP_VIDEO_FMT_RGB565 ? 2 : 3),
                                    data_cache_line_size), /**< 输出缓冲区大小（字节） */
        .out.pic_w = display_width,                /**< 输出图像宽度 */
        .out.pic_h = display_height,               /**< 输出图像高度 */
        .out.block_offset_x = 0,                   /**< 输出起始水平位置 */
        .out.block_offset_y = 0,                   /**< 输出起始垂直位置 */
        .out.srm_cm = APP_VIDEO_FMT == APP_VIDEO_FMT_RGB565 ? PPA_SRM_COLOR_MODE_RGB565 : PPA_SRM_COLOR_MODE_RGB888,

        /* 变换参数 */
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_0, /**< 旋转角度：0度 */
        .scale_x = 1,                               /**< 水平缩放因子：1（不缩放） */
        .scale_y = 1,                               /**< 垂直缩放因子：1（不缩放） */
        .mirror_x = 0,                              /**< 水平镜像：关闭 */
        .mirror_y = 0,                              /**< 垂直镜像：关闭 */
        .rgb_swap = 0,                              /**< RGB通道交换：关闭 */
        .byte_swap = 0,                             /**< 字节交换：关闭 */
        .mode = PPA_TRANS_MODE_BLOCKING,            /**< 传输模式：阻塞模式 */
    };

    /* 调用PPA执行图像缩放旋转镜像操作 */
    esp_err_t ret = ppa_do_scale_rotate_mirror(ppa_srm_handle, &srm_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "PPA SRM failed: %d", ret);
        return;
    }

    /* 计算实际绘制区域大小 */
    uint32_t draw_w = (camera_buf_hes > display_width) ? display_width : camera_buf_hes;
    uint32_t draw_h = (camera_buf_ves > display_height) ? display_height : camera_buf_ves;

    /* 使用dummy draw方式将图像显示到LCD */
    if (dummy_draw_enabled && !dummy_mode_delay_flag)
    {
        ret = esp_lv_adapter_dummy_draw_blit(
            disp,             /**< LVGL显示句柄 */
            0, 0,             /**< 绘制起始坐标（左上角） */
            draw_w,           /**< 绘制宽度 */
            draw_h,           /**< 绘制高度 */
            lcd_buffer[camera_buf_index],  /**< 图像数据缓冲区 */
            true);            /**< 是否等待VSYNC同步 */

        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Dummy draw blit failed: %d", ret);
        }
    }
}
