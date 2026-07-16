/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file cmd_i2ctools.h
 * @brief I2C工具命令头文件
 * 
 * 本文件声明了I2C工具命令的接口函数和全局变量。
 */

#pragma once

/* ESP-IDF组件头文件 */
#include "driver/i2c_master.h"  /* I2C主机驱动API */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册所有I2C工具命令
 * 
 * 调用此函数注册以下命令到ESP控制台：
 * - i2cconfig: 配置I2C总线参数
 * - i2cdetect: 扫描I2C总线上的设备
 * - i2cget: 读取指定设备的寄存器值
 * - i2cset: 写入指定设备的寄存器值
 * - i2cdump: 转储设备所有寄存器内容
 */
void register_i2ctools(void);

/**
 * @brief I2C主机总线句柄
 * 
 * 全局变量，用于所有I2C操作。在初始化时通过i2c_new_master_bus()创建，
 * 在i2cconfig命令中可以重新配置。
 */
extern i2c_master_bus_handle_t tool_bus_handle;

#ifdef __cplusplus
}
#endif