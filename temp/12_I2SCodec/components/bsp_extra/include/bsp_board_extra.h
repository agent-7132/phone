/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bsp_board_extra.h
 * @brief BSP额外音频功能头文件
 * 
 * 本头文件声明了ESP32-P4 BSP额外的音频功能接口，包括：
 * - 音频编解码器初始化和配置
 * - 音量控制
 * - I2S数据读写操作
 * 
 * 这些接口封装了底层音频驱动，提供统一的音频操作接口。
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "driver/i2s_std.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CODEC_DEFAULT_SAMPLE_RATE 16000
#define CODEC_DEFAULT_BIT_WIDTH   16
#define CODEC_DEFAULT_CHANNEL     I2S_SLOT_MODE_STEREO
#define CODEC_DEFAULT_VOLUME      60

/**
 * @brief 初始化音频编解码器
 * 
 * 初始化扬声器和麦克风编解码器设备，设置默认采样率、位宽和通道数。
 * 
 * @return 
 *          - ESP_OK 初始化成功
 *          - ESP_FAIL 初始化失败
 */
esp_err_t bsp_extra_codec_init(void);

/**
 * @brief 设置音频编解码器采样格式
 * 
 * 配置编解码器的采样率、位宽和通道数。需要先关闭已打开的设备，
 * 重新配置后再打开。
 * 
 * @param rate 采样率（Hz）
 * @param bits_cfg 位宽（bits）
 * @param ch 通道模式（单声道/立体声）
 * 
 * @return 
 *          - ESP_OK 设置成功
 *          - ESP_ERR_INVALID_STATE 编解码器未初始化
 */
esp_err_t bsp_extra_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch);

/**
 * @brief 设置扬声器音量
 * 
 * 调整扬声器输出音量。
 * 
 * @param volume 音量值（0-100）
 * @param volume_set 输出参数，返回实际设置的音量值（可选）
 * 
 * @return 
 *          - ESP_OK 设置成功
 *          - ESP_ERR_INVALID_STATE 扬声器编解码器未初始化
 */
esp_err_t bsp_extra_codec_volume_set(int volume, int *volume_set);

/**
 * @brief 从麦克风读取音频数据
 * 
 * 通过I2S接口从麦克风采集音频数据。
 * 
 * @param audio_buffer 音频数据缓冲区指针
 * @param len 要读取的字节数
 * @param bytes_read 输出参数，返回实际读取的字节数（可选）
 * @param timeout_ms 超时时间（毫秒，未使用）
 * 
 * @return 
 *          - ESP_OK 读取成功
 *          - ESP_ERR_INVALID_STATE 麦克风编解码器未初始化
 */
esp_err_t bsp_extra_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms);

/**
 * @brief 向扬声器写入音频数据
 * 
 * 通过I2S接口向扬声器播放音频数据。
 * 
 * @param audio_buffer 音频数据缓冲区指针
 * @param len 要写入的字节数
 * @param bytes_written 输出参数，返回实际写入的字节数（可选）
 * @param timeout_ms 超时时间（毫秒，未使用）
 * 
 * @return 
 *          - ESP_OK 写入成功
 *          - ESP_ERR_INVALID_STATE 扬声器编解码器未初始化
 */
esp_err_t bsp_extra_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
