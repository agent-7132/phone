/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bsp_extra_audio.c
 * @brief BSP额外音频功能实现文件
 * 
 * 本文件实现了ESP32-P4 BSP额外的音频功能，包括：
 * - 音频编解码器初始化和采样格式配置
 * - 扬声器音量控制
 * - I2S音频数据的读取和写入
 * 
 * 使用ESP-IDF的esp_codec_dev组件进行底层音频设备管理。
 */

#include <stdbool.h>
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_log.h"

static const char *TAG = "bsp_extra_audio";

static esp_codec_dev_handle_t s_play_dev;
static esp_codec_dev_handle_t s_record_dev;
static bool s_audio_initialized;
static bool s_play_opened;
static bool s_record_opened;
static int s_volume = CODEC_DEFAULT_VOLUME;

/**
 * @brief 设置音频编解码器采样格式
 * 
 * 配置编解码器的采样率、位宽和通道数。执行流程：
 * 1. 检查编解码器是否已初始化
 * 2. 如果设备已打开，先关闭设备
 * 3. 配置新的采样格式
 * 4. 重新打开设备
 * 
 * @param rate 采样率（Hz）
 * @param bits_cfg 位宽（bits）
 * @param ch 通道模式（单声道/立体声）
 * 
 * @return 
 *          - ESP_OK 设置成功
 *          - ESP_ERR_INVALID_STATE 编解码器未初始化
 */
esp_err_t bsp_extra_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    ESP_RETURN_ON_FALSE(s_play_dev || s_record_dev, ESP_ERR_INVALID_STATE, TAG, "codec is not initialized");

    esp_err_t ret = ESP_OK;
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = rate,
        .channel = ch,
        .bits_per_sample = bits_cfg,
    };

    if (s_play_opened) {
        ret |= esp_codec_dev_close(s_play_dev);
        s_play_opened = false;
    }
    if (s_record_opened) {
        ret |= esp_codec_dev_close(s_record_dev);
        s_record_opened = false;
    }

    if (s_play_dev) {
        ret |= esp_codec_dev_open(s_play_dev, &fs);
        if (ret == ESP_OK) {
            s_play_opened = true;
        }
    }
    if (s_record_dev) {
        ret |= esp_codec_dev_set_in_gain(s_record_dev, 24.0);
        ret |= esp_codec_dev_open(s_record_dev, &fs);
        if (ret == ESP_OK) {
            s_record_opened = true;
        }
    }

    return ret;
}

/**
 * @brief 设置扬声器音量
 * 
 * 调整扬声器输出音量，并保存当前音量值。
 * 
 * @param volume 音量值（0-100）
 * @param volume_set 输出参数，返回实际设置的音量值（可选）
 * 
 * @return 
 *          - ESP_OK 设置成功
 *          - ESP_ERR_INVALID_STATE 扬声器编解码器未初始化
 */
esp_err_t bsp_extra_codec_volume_set(int volume, int *volume_set)
{
    ESP_RETURN_ON_FALSE(s_play_dev, ESP_ERR_INVALID_STATE, TAG, "speaker codec is not initialized");
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(s_play_dev, volume), TAG, "set codec volume failed");

    s_volume = volume;
    if (volume_set) {
        *volume_set = s_volume;
    }
    return ESP_OK;
}

/**
 * @brief 初始化音频编解码器
 * 
 * 初始化扬声器和麦克风编解码器设备，执行流程：
 * 1. 检查是否已初始化，避免重复初始化
 * 2. 初始化扬声器编解码器
 * 3. 初始化麦克风编解码器
 * 4. 设置默认采样格式
 * 5. 设置默认音量
 * 6. 标记初始化完成
 * 
 * @return 
 *          - ESP_OK 初始化成功
 *          - ESP_FAIL 扬声器或麦克风初始化失败
 */
esp_err_t bsp_extra_codec_init(void)
{
    if (s_audio_initialized) {
        return ESP_OK;
    }

    s_play_dev = bsp_audio_codec_speaker_init();
    ESP_RETURN_ON_FALSE(s_play_dev, ESP_FAIL, TAG, "speaker codec init failed");

    s_record_dev = bsp_audio_codec_microphone_init();
    ESP_RETURN_ON_FALSE(s_record_dev, ESP_FAIL, TAG, "microphone codec init failed");

    ESP_RETURN_ON_ERROR(bsp_extra_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE, CODEC_DEFAULT_BIT_WIDTH, CODEC_DEFAULT_CHANNEL), TAG, "set codec format failed");
    ESP_RETURN_ON_ERROR(bsp_extra_codec_volume_set(CODEC_DEFAULT_VOLUME, NULL), TAG, "set codec volume failed");

    s_audio_initialized = true;
    return ESP_OK;
}

/**
 * @brief 从麦克风读取音频数据
 * 
 * 通过编解码器接口从麦克风采集音频数据。
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
esp_err_t bsp_extra_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms)
{
    (void)timeout_ms;
    ESP_RETURN_ON_FALSE(s_record_dev, ESP_ERR_INVALID_STATE, TAG, "microphone codec is not initialized");

    esp_err_t ret = esp_codec_dev_read(s_record_dev, audio_buffer, len);
    if (bytes_read) {
        *bytes_read = (ret == ESP_OK) ? len : 0;
    }
    return ret;
}

/**
 * @brief 向扬声器写入音频数据
 * 
 * 通过编解码器接口向扬声器播放音频数据。
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
esp_err_t bsp_extra_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    (void)timeout_ms;
    ESP_RETURN_ON_FALSE(s_play_dev, ESP_ERR_INVALID_STATE, TAG, "speaker codec is not initialized");

    esp_err_t ret = esp_codec_dev_write(s_play_dev, audio_buffer, len);
    if (bytes_written) {
        *bytes_written = (ret == ESP_OK) ? len : 0;
    }
    return ret;
}
