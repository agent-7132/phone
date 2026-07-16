/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/**
 * @file ethernet_init.h
 * @brief 以太网初始化组件头文件
 * 
 * 本头文件声明了以太网驱动的初始化和反初始化函数，提供统一的接口
 * 来管理内部以太网控制器和SPI以太网模块。
 */

#pragma once

#include "esp_eth_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 以太网驱动初始化函数
 * 
 * 根据ESP-IDF配置初始化一个或多个以太网接口，支持：
 * - 内部以太网控制器（EMAC）
 * - SPI以太网模块（KSZ8851SNL、DM9051、W5500）
 * - 单端口和多端口配置
 * 
 * 初始化完成后，返回以太网驱动句柄数组和接口数量，供应用程序使用。
 * 
 * @param[out] eth_handles_out 输出参数，指向以太网驱动句柄数组的指针
 * @param[out] eth_cnt_out 输出参数，返回初始化的以太网接口数量
 * 
 * @return 
 *          - ESP_OK 初始化成功
 *          - ESP_ERR_INVALID_ARG 参数无效（传入NULL指针）
 *          - ESP_ERR_NO_MEM 内存分配失败
 *          - ESP_FAIL 其他初始化失败（如驱动安装失败）
 */
esp_err_t example_eth_init(esp_eth_handle_t *eth_handles_out[], uint8_t *eth_cnt_out);

/**
 * @brief 以太网驱动反初始化函数
 * 
 * 释放以太网驱动相关资源，包括：
 * - 卸载以太网驱动
 * - 释放MAC和PHY实例
 * - 释放SPI总线资源（如果使用SPI以太网）
 * - 释放以太网句柄数组内存
 * 
 * @note 调用此函数前，所有以太网驱动必须已通过esp_eth_stop()停止
 * 
 * @param[in] eth_handles 以太网驱动句柄数组
 * @param[in] eth_cnt 以太网接口数量
 * 
 * @return 
 *          - ESP_OK 反初始化成功
 *          - ESP_ERR_INVALID_ARG 参数无效（传入NULL指针）
 */
esp_err_t example_eth_deinit(esp_eth_handle_t *eth_handles, uint8_t eth_cnt);

#ifdef __cplusplus
}
#endif
