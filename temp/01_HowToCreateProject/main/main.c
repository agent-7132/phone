/**
 * @file main.c
 * @brief ESP-IDF 项目入口文件
 * 
 * 该文件是 ESP-IDF 应用程序的核心入口，包含应用程序的主函数 app_main。
 * 所有 ESP-IDF 应用程序都必须实现 app_main 函数，作为 FreeRTOS 任务启动后
 * 的第一个执行点。
 * 
 * @copyright Copyright (c) 2024 Espressif Systems (Shanghai) Co., Ltd.
 * @license Licensed under Apache License 2.0 (see LICENSE file)
 * @version 1.0.0
 * @date 2024-01-01
 */

/**
 * @brief 标准输入输出库
 * 
 * 提供 printf、scanf 等标准 I/O 函数，用于调试信息输出和用户输入处理。
 * 在 ESP-IDF 环境中，printf 输出默认重定向到 UART0（串口）。
 */
#include <stdio.h>

/**
 * @brief ESP-IDF 应用程序主入口函数
 * 
 * app_main 是 ESP-IDF 应用程序的入口点，由 ESP-IDF 启动流程自动调用。
 * 该函数在 FreeRTOS 调度器启动后作为第一个任务执行。
 * 
 * 函数执行流程：
 * 1. 系统初始化完成后，ESP-IDF 框架创建主任务并调用此函数
 * 2. 用户应在此函数中初始化硬件、创建任务、启动应用逻辑
 * 3. 函数返回后，主任务结束，但其他已创建的任务继续运行
 * 
 * @note app_main 函数不能有参数，不能返回值
 * @note 不要在此函数中执行长时间阻塞操作，应创建独立任务处理
 * @note 该函数执行完成后不会导致应用程序退出，其他任务会继续运行
 */
void app_main(void)
{
    /**
     * @brief 应用程序主循环（可选）
     * 
     * 用户可以在此处添加应用程序的主逻辑。常见做法包括：
     * - 初始化外设驱动（如 LCD、传感器、WiFi 等）
     * - 创建 FreeRTOS 任务处理不同功能
     * - 启动事件循环或消息队列
     * 
     * 当前示例为空实现，用户可根据需求添加代码。
     */
}
