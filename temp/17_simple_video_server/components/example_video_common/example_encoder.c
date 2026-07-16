/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

/**
 * @file example_encoder.c
 * @brief JPEG 编码器封装模块
 *
 * 本文件实现了 JPEG 编码器的统一封装层，支持两种编码方式：
 * - 硬件编码：使用 ESP32-P4 内置的 JPEG 编码引擎（CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER）
 * - 软件编码：使用 ESP-IDF 提供的软件 JPEG 编码器（esp_jpeg_enc）
 *
 * 设计特点：
 * 1. 硬件编码器采用单例模式（全局共享一个编码引擎实例），通过引用计数管理生命周期
 * 2. 软件编码器每个摄像头独立一个实例
 * 3. 支持多种像素格式输入：RGB565、RGB24、UYVY、YUYV、YUV420、YUV444、GREY、SBGGR8
 * 4. 统一的 API 接口，上层无需关心具体编码方式
 *
 * 编码器初始化流程：
 * ┌──────────────────────────────────────────────────────────────┐
 * │  example_encoder_init()                                      │
 * │      │                                                       │
 * │      ▼                                                       │
 * │  ┌─────────────────────────────────────────────────────┐     │
 * │  │  根据像素格式选择编码器配置                         │     │
 * │  │  (src_type, sub_sample, input_size)                │     │
 * │  └────────────────────┬──────────────────────────────┘     │
 * │                       │                                      │
 * │        ┌──────────────┼──────────────┐                       │
 * │        ▼              ▼              ▼                       │
 * │   [HW Encoder]   [SW Encoder]   [Unsupported]                │
 * │        │              │              │                       │
 * │        ▼              ▼              ▼                       │
 * │  jpeg_new_encoder_engine  jpeg_enc_open    ESP_ERR_NOT_SUPPORTED │
 * │        │              │                                      │
 * │        └──────────────┼──────────────┘                       │
 * │                       ▼                                      │
 * │            分配 example_encoder_t 结构体                      │
 * │                       │                                      │
 * │                       ▼                                      │
 * │            设置引用计数/保存句柄                              │
 * └──────────────────────────────────────────────────────────────┘
 */

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_video_ioctl.h"
#include "esp_video_init.h"
#include "esp_cam_sensor_xclk.h"
#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
#include "driver/jpeg_encode.h"
#else
#include "esp_jpeg_enc.h"
#endif
#include "example_video_common.h"

/**
 * @struct example_encoder
 * @brief 编码器内部状态结构
 *
 * 封装了编码器的所有状态信息，包括编码配置、输出缓冲区大小等。
 * 根据编译配置（CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER），内部成员不同：
 * - 硬件编码：存储 jpeg_encode_cfg_t 配置和输出缓冲区大小
 * - 软件编码：存储 jpeg_enc_handle_t 句柄和输出缓冲区大小
 */
typedef struct example_encoder {
#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    jpeg_encode_cfg_t jpeg_enc_config;   /**< 硬件编码配置结构 */
#else
    jpeg_enc_handle_t jpeg_handle;       /**< 软件编码器句柄 */
#endif
    uint32_t jpeg_out_buf_size;          /**< JPEG 输出缓冲区大小（字节） */
} example_encoder_t;

/**
 * @var TAG
 * @brief 日志标签
 */
static const char *TAG = "example_encoder";

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
/**
 * @var s_jpeg_hw_handle
 * @brief 硬件 JPEG 编码器引擎句柄（全局单例）
 *
 * 硬件编码器引擎是全局共享的，所有视频流共用同一个引擎实例，
 * 通过引用计数管理生命周期，避免重复创建和销毁。
 */
static jpeg_encoder_handle_t s_jpeg_hw_handle;

/**
 * @var s_jpeg_hw_ref_count
 * @brief 硬件编码器引用计数
 *
 * 记录当前使用硬件编码器的视频流数量。当引用计数从 0 变为 1 时创建引擎，
 * 从 1 变为 0 时销毁引擎。
 */
static uint32_t s_jpeg_hw_ref_count;
#endif

/**
 * @brief 初始化 JPEG 编码器
 *
 * 根据编译配置选择硬件或软件编码方式，完成以下初始化步骤：
 * 1. 验证输入参数有效性
 * 2. 根据像素格式配置编码器参数（源格式、子采样方式、输入数据大小）
 * 3. 创建编码器实例（硬件引擎或软件编码器）
 * 4. 分配编码器内部状态结构
 * 5. 设置引用计数（硬件编码）或保存句柄（软件编码）
 * 6. 计算输出缓冲区大小
 *
 * 支持的像素格式及对应编码配置：
 * ┌──────────────────┬─────────────────────────┬────────────────────┐
 * │ 像素格式         │ 源类型                  │ 子采样方式         │
 * ├──────────────────┼─────────────────────────┼────────────────────┤
 * │ V4L2_PIX_FMT_GREY│ GRAY                    │ GRAY               │
 * │ V4L2_PIX_FMT_SBGGR8│ GRAY (测试用)         │ GRAY               │
 * │ V4L2_PIX_FMT_RGB565│ RGB565               │ YUV422             │
 * │ V4L2_PIX_FMT_RGB24│ RGB888                │ YUV444             │
 * │ V4L2_PIX_FMT_UYVY│ YUV422               │ YUV422             │
 * │ V4L2_PIX_FMT_YUV420│ YUV420 (ESP32-P4)    │ YUV420             │
 * │ V4L2_PIX_FMT_YUV444│ YUV444 (ESP32-P4)    │ YUV444             │
 * │ V4L2_PIX_FMT_YUYV│ YCbYCr (软件编码)     │ 422                │
 * │ V4L2_PIX_FMT_RGB565X│ RGB565_BE (软件编码) │ 422                │
 * └──────────────────┴─────────────────────────┴────────────────────┘
 *
 * @param config 编码器配置结构指针，包含图像宽度、高度、像素格式和质量
 * @param ret_handle 输出参数，返回编码器句柄
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数无效，ESP_ERR_NO_MEM 内存分配失败，
 *         ESP_ERR_NOT_SUPPORTED 不支持的像素格式，其他错误码表示编码器创建失败
 */
esp_err_t example_encoder_init(example_encoder_config_t *config, example_encoder_handle_t *ret_handle)
{
    /* 参数有效性检查 */
    if (!config || !ret_handle) {
        ESP_LOGE(TAG, "invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    example_encoder_t *encoder;
    uint32_t jpeg_enc_input_src_size;

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    /* 硬件编码：使用 jpeg_encode_cfg_t 配置结构 */
    jpeg_encode_cfg_t jpeg_enc_config = {0};
    jpeg_encoder_handle_t jpeg_handle = NULL;
#else
    /* 软件编码：使用 jpeg_enc_config_t 配置结构 */
    jpeg_enc_handle_t jpeg_handle = NULL;
    jpeg_enc_config_t jpeg_enc_config = {0};
#endif

    /* 设置图像尺寸 */
    jpeg_enc_config.height = config->height;
    jpeg_enc_config.width = config->width;

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    /* 硬件编码：设置图像质量 */
    jpeg_enc_config.image_quality = config->quality;

    /* 根据像素格式选择编码配置 */
    switch (config->pixel_format) {
    case V4L2_PIX_FMT_SBGGR8:
    case V4L2_PIX_FMT_GREY:
        /* 灰度格式：将 raw8 视为灰度处理（仅用于测试） */
        jpeg_enc_config.src_type = JPEG_ENCODE_IN_FORMAT_GRAY;
        jpeg_enc_config.sub_sample = JPEG_DOWN_SAMPLING_GRAY;
        jpeg_enc_input_src_size = config->width * config->height;
        break;
    case V4L2_PIX_FMT_RGB565:
        /* RGB565 小端格式 */
        jpeg_enc_config.src_type = JPEG_ENCODE_IN_FORMAT_RGB565;
        jpeg_enc_config.sub_sample = JPEG_DOWN_SAMPLING_YUV422;
        jpeg_enc_input_src_size = config->width * config->height * 2;
        break;
    case V4L2_PIX_FMT_RGB24:
        /* RGB888 格式 */
        jpeg_enc_config.src_type = JPEG_ENCODE_IN_FORMAT_RGB888;
        jpeg_enc_config.sub_sample = JPEG_DOWN_SAMPLING_YUV444;
        jpeg_enc_input_src_size = config->width * config->height * 3;
        break;
    case V4L2_PIX_FMT_UYVY:
        /* YUV422 UYVY 格式 */
        jpeg_enc_config.src_type = JPEG_ENCODE_IN_FORMAT_YUV422;
        jpeg_enc_config.sub_sample = JPEG_DOWN_SAMPLING_YUV422;
        jpeg_enc_input_src_size = config->width * config->height * 2;
        break;
#if CONFIG_ESP32P4_REV_MIN_FULL >= 300
    case V4L2_PIX_FMT_YUV420:
        /* YUV420 格式（ESP32-P4 修订版 >= 300 支持） */
        jpeg_enc_config.src_type = JPEG_ENCODE_IN_FORMAT_YUV420;
        jpeg_enc_config.sub_sample = JPEG_DOWN_SAMPLING_YUV420;
        jpeg_enc_input_src_size = config->width * config->height * 3 / 2;
        break;
    case V4L2_PIX_FMT_YUV444:
        /* YUV444 格式（ESP32-P4 修订版 >= 300 支持） */
        jpeg_enc_config.src_type = JPEG_ENCODE_IN_FORMAT_YUV444;
        jpeg_enc_config.sub_sample = JPEG_DOWN_SAMPLING_YUV444;
        jpeg_enc_input_src_size = config->width * config->height * 3;
        break;
#endif
    default:
        /* 不支持的像素格式 */
        ESP_LOGE(TAG, "Unsupported format");
        return ESP_ERR_NOT_SUPPORTED;
    }

    /* 硬件编码器：按需创建编码引擎（单例模式） */
    if (!s_jpeg_hw_handle) {
        jpeg_encode_engine_cfg_t encode_eng_cfg = {
            .timeout_ms = 5000,  /* 编码超时时间（毫秒） */
        };
        ESP_RETURN_ON_ERROR(jpeg_new_encoder_engine(&encode_eng_cfg, &jpeg_handle), TAG, "failed to create jpeg encoder engine");
    }
#else
    /* 软件编码：设置图像质量 */
    jpeg_enc_config.quality = config->quality;

    /* 根据像素格式选择编码配置 */
    switch (config->pixel_format) {
    case V4L2_PIX_FMT_SBGGR8:
    case V4L2_PIX_FMT_GREY:
        /* 灰度格式 */
        jpeg_enc_config.src_type = JPEG_PIXEL_FORMAT_GRAY;
        jpeg_enc_config.subsampling = JPEG_SUBSAMPLE_GRAY;
        jpeg_enc_input_src_size = config->width * config->height;
        break;
    case V4L2_PIX_FMT_UYVY:
        /* YUV422 UYVY 格式 */
        jpeg_enc_config.src_type = JPEG_PIXEL_FORMAT_CbYCrY;
        jpeg_enc_config.subsampling = JPEG_SUBSAMPLE_422;
        jpeg_enc_input_src_size = config->width * config->height * 2;
        break;
    case V4L2_PIX_FMT_YUYV:
        /* YUV422 YUYV 格式 */
        jpeg_enc_config.src_type = JPEG_PIXEL_FORMAT_YCbYCr;
        jpeg_enc_config.subsampling = JPEG_SUBSAMPLE_422;
        jpeg_enc_input_src_size = config->width * config->height * 2;
        break;
    case V4L2_PIX_FMT_RGB565:
        /* RGB565 小端格式 */
        jpeg_enc_config.src_type = JPEG_PIXEL_FORMAT_RGB565_LE;
        jpeg_enc_config.subsampling = JPEG_SUBSAMPLE_422;
        jpeg_enc_input_src_size = config->width * config->height * 2;
        break;
    case V4L2_PIX_FMT_RGB565X:
        /* RGB565 大端格式 */
        jpeg_enc_config.src_type = JPEG_PIXEL_FORMAT_RGB565_BE;
        jpeg_enc_config.subsampling = JPEG_SUBSAMPLE_422;
        jpeg_enc_input_src_size = config->width * config->height * 2;
        break;
    default:
        /* 不支持的像素格式 */
        ESP_LOGE(TAG, "Unsupported format");
        return ESP_ERR_NOT_SUPPORTED;
    }

    /* 软件编码器：创建编码器实例 */
    ESP_RETURN_ON_ERROR(jpeg_enc_open(&jpeg_enc_config, &jpeg_handle), TAG, "failed to open jpeg encoder");
#endif

    /* 分配编码器内部状态结构 */
    encoder = (example_encoder_t *)calloc(1, sizeof(example_encoder_t));
    ESP_GOTO_ON_FALSE(encoder, ESP_ERR_NO_MEM, fail0, TAG, "failed to alloc example encoder");

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    /* 硬件编码：保存配置并更新引用计数 */
    encoder->jpeg_enc_config = jpeg_enc_config;
    if (!s_jpeg_hw_ref_count) {
        s_jpeg_hw_ref_count++;
        s_jpeg_hw_handle = jpeg_handle;
    }
#else
    /* 软件编码：保存编码器句柄 */
    encoder->jpeg_handle = jpeg_handle;
#endif

    /* 计算输出缓冲区大小：取输入数据大小的 3/4 作为估算值 */
    /* 注意：JPEG_ENC_QUALITY 越大图像质量越好，需要更大的缓冲区 */
    encoder->jpeg_out_buf_size = jpeg_enc_input_src_size * 3 / 4;

    /* 返回编码器句柄 */
    *ret_handle = encoder;

    return ESP_OK;

fail0:
    /* 错误处理：释放已创建的编码器资源 */
#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    jpeg_del_encoder_engine(jpeg_handle);
#else
    jpeg_enc_close(jpeg_handle);
#endif
    return ret;
}

/**
 * @brief 分配编码器输出缓冲区
 *
 * 根据编码器配置分配适当大小的 JPEG 输出缓冲区。
 * - 硬件编码：使用 jpeg_alloc_encoder_mem() 分配专用内存
 * - 软件编码：使用 jpeg_calloc_align() 分配对齐内存（128 字节对齐）
 *
 * @note JPEG_ENC_QUALITY 越大图像质量越好，需要更大的缓冲区空间。
 *
 * @param handle 编码器句柄
 * @param buf 输出参数，返回分配的缓冲区指针
 * @param size 输出参数，返回缓冲区大小（字节）
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数无效，ESP_ERR_INVALID_STATE 编码器未初始化，
 *         ESP_ERR_NO_MEM 内存分配失败
 */
esp_err_t example_encoder_alloc_output_buffer(example_encoder_handle_t handle, uint8_t **buf, uint32_t *size)
{
#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    /* 硬件编码：检查编码器是否已初始化 */
    if (!s_jpeg_hw_ref_count) {
        ESP_LOGE(TAG, "jpeg hardware encoder is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
#endif

    uint8_t *jpeg_out_buf;
    example_encoder_t *encoder = (example_encoder_t *)handle;

    /* 参数有效性检查 */
    if (!encoder || !buf || !size) {
        ESP_LOGE(TAG, "invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    /* 硬件编码：使用专用内存分配函数 */
    size_t jpeg_out_buf_size;
    jpeg_encode_memory_alloc_cfg_t jpeg_enc_output_mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,  /* 分配输出缓冲区 */
    };

    /* 分配编码器输出缓冲区 */
    jpeg_out_buf = (uint8_t *)jpeg_alloc_encoder_mem(encoder->jpeg_out_buf_size, &jpeg_enc_output_mem_cfg, &jpeg_out_buf_size);
    ESP_RETURN_ON_FALSE(jpeg_out_buf, ESP_ERR_NO_MEM, TAG, "failed to alloc jpeg output buf");
#else
    /* 软件编码：使用对齐内存分配 */
    uint32_t jpeg_out_buf_size;

    /* 分配 128 字节对齐的缓冲区 */
    jpeg_out_buf = jpeg_calloc_align(encoder->jpeg_out_buf_size, 128);
    ESP_RETURN_ON_FALSE(jpeg_out_buf, ESP_ERR_NO_MEM, TAG, "failed to alloc jpeg output buf");
    jpeg_out_buf_size = encoder->jpeg_out_buf_size;
#endif

    /* 返回缓冲区指针和大小 */
    *buf = jpeg_out_buf;
    *size = jpeg_out_buf_size;

    return ESP_OK;
}

/**
 * @brief 释放编码器输出缓冲区
 *
 * 根据编码方式选择对应的内存释放函数：
 * - 硬件编码：使用标准 free() 释放
 * - 软件编码：使用 jpeg_free_align() 释放对齐内存
 *
 * @param handle 编码器句柄
 * @param buf 要释放的缓冲区指针
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数无效，ESP_ERR_INVALID_STATE 编码器未初始化
 */
esp_err_t example_encoder_free_output_buffer(example_encoder_handle_t handle, uint8_t *buf)
{
#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    /* 硬件编码：检查编码器是否已初始化 */
    if (!s_jpeg_hw_ref_count) {
        ESP_LOGE(TAG, "jpeg hardware encoder is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
#endif

    /* 参数有效性检查 */
    if (!buf || !handle) {
        ESP_LOGE(TAG, "invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    /* 硬件编码：使用标准 free */
    free(buf);
#else
    /* 软件编码：使用对齐内存释放函数 */
    jpeg_free_align(buf);
#endif
    return ESP_OK;
}

/**
 * @brief 执行 JPEG 编码
 *
 * 将原始图像数据编码为 JPEG 格式。调用前需确保：
 * 1. 编码器已初始化（example_encoder_init）
 * 2. 输出缓冲区已分配（example_encoder_alloc_output_buffer）
 *
 * @param handle 编码器句柄
 * @param src_buf 源图像数据缓冲区指针
 * @param src_size 源数据大小（字节）
 * @param dst_buf JPEG 输出缓冲区指针
 * @param dst_size 输出缓冲区大小（字节）
 * @param dst_size_out 输出参数，返回实际编码后的数据大小（字节）
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数无效，ESP_ERR_INVALID_STATE 编码器未初始化，
 *         其他错误码表示编码失败
 */
esp_err_t example_encoder_process(example_encoder_handle_t handle, uint8_t *src_buf, uint32_t src_size,
                                  uint8_t *dst_buf, uint32_t dst_size, uint32_t *dst_size_out)
{
#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    /* 硬件编码：检查编码器是否已初始化 */
    if (!s_jpeg_hw_ref_count) {
        ESP_LOGE(TAG, "jpeg hardware encoder is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
#endif

    /* 参数有效性检查 */
    if (!handle || !src_buf || !src_size || !dst_buf || !dst_size || !dst_size_out) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    example_encoder_t *encoder = (example_encoder_t *)handle;

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    /* 硬件编码：调用硬件编码引擎处理 */
    ret = jpeg_encoder_process(s_jpeg_hw_handle, &encoder->jpeg_enc_config, src_buf, src_size, dst_buf, dst_size, dst_size_out);
#else
    /* 软件编码：调用软件编码器处理 */
    ret = jpeg_enc_process(encoder->jpeg_handle, src_buf, src_size, dst_buf, dst_size, (int *)dst_size_out);
#endif

    return ret;
}

/**
 * @brief 设置 JPEG 编码质量
 *
 * 动态调整 JPEG 编码质量。质量值范围通常为 1-100，值越大图像质量越高。
 *
 * @param handle 编码器句柄
 * @param quality JPEG 质量值（1-100）
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数无效
 */
esp_err_t example_encoder_set_jpeg_quality(example_encoder_handle_t handle, uint8_t quality)
{
    esp_err_t ret = ESP_OK;
    example_encoder_t *encoder = (example_encoder_t *)handle;

    /* 参数有效性检查 */
    if (!encoder) {
        ESP_LOGE(TAG, "example encoder is not initialized");
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    /* 硬件编码：直接修改配置结构中的质量参数 */
    encoder->jpeg_enc_config.image_quality = quality;
#else
    /* 软件编码：调用编码器 API 设置质量 */
    ret = jpeg_enc_set_quality(encoder->jpeg_handle, quality);
#endif
    return ret;
}

/**
 * @brief 反初始化编码器
 *
 * 释放编码器相关资源：
 * - 硬件编码：递减引用计数，当引用计数归零时销毁编码引擎
 * - 软件编码：关闭编码器实例
 *
 * @param handle 编码器句柄
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数无效
 */
esp_err_t example_encoder_deinit(example_encoder_handle_t handle)
{
    example_encoder_t *encoder = (example_encoder_t *)handle;

    /* 参数有效性检查 */
    if (!encoder) {
        ESP_LOGE(TAG, "example encoder is not initialized");
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    /* 硬件编码：管理引用计数 */
    if (s_jpeg_hw_ref_count) {
        s_jpeg_hw_ref_count--;
        /* 当引用计数归零时销毁编码引擎 */
        if (!s_jpeg_hw_ref_count) {
            jpeg_del_encoder_engine(s_jpeg_hw_handle);
            s_jpeg_hw_handle = NULL;
        }
    } else {
        /* 引用计数已为零，可能存在重复反初始化 */
        ESP_LOGW(TAG, "jpeg hardware encoder ref count already 0, possible double deinit");
    }
#else
    /* 软件编码：关闭编码器实例 */
    jpeg_enc_close(encoder->jpeg_handle);
#endif

    /* 释放编码器内部状态结构 */
    free(encoder);

    return ESP_OK;
}
