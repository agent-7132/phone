/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/**
 * @file sd_test_io.h
 * @brief SD卡引脚测试函数头文件
 * 
 * 本文件声明了SD卡引脚连接测试的接口函数和数据结构。
 */

#pragma once

/* 标准库头文件 */
#include <stdint.h>              /* 标准整数类型定义 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SD卡引脚配置结构体
 * 
 * 用于配置SD卡引脚测试所需的引脚信息，包括：
 * - 引脚名称数组
 * - GPIO引脚编号数组
 * - ADC通道数组（可选，用于电压测量）
 */
typedef struct {
    const char** names;           /* 引脚名称数组（如"CLK", "CMD", "D0"等） */
    const int* pins;              /* GPIO引脚编号数组 */
#if CONFIG_EXAMPLE_ENABLE_ADC_FEATURE
    const int *adc_channels;      /* ADC通道数组，用于测量引脚电压（可选） */
#endif
} pin_configuration_t;

/**
 * @brief 检查SD卡引脚连接函数
 * 
 * 执行SD卡引脚连接测试，包括：
 * 1. 测试引脚恢复时间（无外部上拉和有内部上拉）
 * 2. 测试引脚电压电平（无外部上拉和有内部上拉）
 * 3. 测试引脚串扰（无外部上拉和有内部上拉）
 * 
 * @param config 引脚配置结构体指针
 * @param pin_count 引脚数量
 */
void check_sd_card_pins(pin_configuration_t *config, const int pin_count);

#ifdef __cplusplus
}
#endif