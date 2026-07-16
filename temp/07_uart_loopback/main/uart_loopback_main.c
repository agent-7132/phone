/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file uart_loopback_main.c
 * @brief UART回环测试示例程序
 * 
 * 本示例演示了ESP32-P4 UART串口的基本收发功能：
 * 1. 配置UART参数（波特率、数据位、校验位、停止位）
 * 2. 安装UART驱动并设置TX/RX引脚
 * 3. 定期发送测试数据到TX引脚
 * 4. 从RX引脚读取回环数据并打印到日志
 * 
 * 使用方法：
 * 1. 通过idf.py menuconfig配置UART端口号、波特率、TX/RX GPIO编号
 * 2. 将程序烧录到开发板
 * 3. 使用杜邦线连接TX GPIO和RX GPIO（形成回环）
 * 4. 打开串口监视器查看收发数据
 * 
 * 关键技术点：
 * - UART驱动安装时配置发送和接收缓冲区大小
 * - 使用uart_write_bytes()发送数据
 * - 使用uart_read_bytes()读取数据（带超时机制）
 */

/* 标准库头文件 */
#include <stdbool.h>            /* 布尔类型定义 */
#include <stdio.h>              /* 标准输入输出函数 */
#include <string.h>             /* 字符串处理函数，如strlen() */

/* ESP-IDF组件头文件 */
#include "driver/uart.h"        /* UART驱动API */
#include "esp_log.h"            /* ESP日志库 */
#include "freertos/FreeRTOS.h"  /* FreeRTOS实时操作系统核心API */
#include "freertos/task.h"      /* FreeRTOS任务管理API */
#include "sdkconfig.h"          /* 项目配置头文件，包含menuconfig配置选项 */

/* 日志标签，用于标识本模块的日志输出 */
static const char *TAG = "uart_loopback";

/* UART端口号，从menuconfig配置中获取 */
static const uart_port_t UART_PORT = CONFIG_EXAMPLE_UART_PORT_NUM;

/* 回环测试发送文本，用于验证UART收发功能 */
static const char *LOOPBACK_TEXT = "hello from esp32-p4 uart loopback\n";

/**
 * @brief ESP-IDF应用程序入口函数
 * 
 * 主函数负责初始化UART并执行回环测试：
 * 1. 定义UART配置参数结构体
 * 2. 打印示例标题和配置信息
 * 3. 安装UART驱动
 * 4. 配置UART参数
 * 5. 设置UART TX/RX引脚
 * 6. 进入循环，定期发送数据并接收回环数据
 * 
 * @return void 无返回值
 */
void app_main(void)
{
    /* UART配置参数结构体 */
    const uart_config_t uart_config = {
        .baud_rate = CONFIG_EXAMPLE_UART_BAUD_RATE,  /* 波特率，从menuconfig配置获取 */
        .data_bits = UART_DATA_8_BITS,               /* 数据位：8位 */
        .parity = UART_PARITY_DISABLE,               /* 校验位：禁用 */
        .stop_bits = UART_STOP_BITS_1,               /* 停止位：1位 */
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,       /* 硬件流控：禁用 */
        .source_clk = UART_SCLK_DEFAULT,             /* 时钟源：默认时钟 */
    };

    /* 打印示例标题和配置信息 */
    printf("\n");
    printf("========================================\n");
    printf(" UART Loopback Demo\n");
    printf("========================================\n");
    printf("Connect TX GPIO %d to RX GPIO %d.\n", CONFIG_EXAMPLE_UART_TX_GPIO, CONFIG_EXAMPLE_UART_RX_GPIO);
    printf("Baud rate: %d\n", CONFIG_EXAMPLE_UART_BAUD_RATE);
    printf("\n");

    /* 安装UART驱动 */
    /* 参数说明：
     *   UART_PORT: UART端口号
     *   1024: 接收缓冲区大小（字节）
     *   1024: 发送缓冲区大小（字节）
     *   0: 事件队列长度（不使用事件队列）
     *   NULL: 事件队列句柄（不使用）
     *   0: 中断标志位（使用默认配置） */
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 1024, 1024, 0, NULL, 0));
    
    /* 配置UART参数 */
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    
    /* 设置UART引脚 */
    /* 参数说明：
     *   UART_PORT: UART端口号
     *   CONFIG_EXAMPLE_UART_TX_GPIO: TX引脚编号
     *   CONFIG_EXAMPLE_UART_RX_GPIO: RX引脚编号
     *   UART_PIN_NO_CHANGE: RTS引脚（不改变）
     *   UART_PIN_NO_CHANGE: CTS引脚（不改变） */
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT,
                                 CONFIG_EXAMPLE_UART_TX_GPIO,
                                 CONFIG_EXAMPLE_UART_RX_GPIO,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));

    /* 接收缓冲区，用于存储从UART读取的数据 */
    uint8_t rx_buffer[128];

    /* 无限循环，持续执行UART回环测试 */
    while (true) {
        /* 发送测试文本到UART */
        /* 参数说明：
         *   UART_PORT: UART端口号
         *   LOOPBACK_TEXT: 要发送的数据指针
         *   strlen(LOOPBACK_TEXT): 要发送的数据长度 */
        uart_write_bytes(UART_PORT, LOOPBACK_TEXT, strlen(LOOPBACK_TEXT));

        /* 从UART读取回环数据 */
        /* 参数说明：
         *   UART_PORT: UART端口号
         *   rx_buffer: 接收数据的缓冲区指针
         *   sizeof(rx_buffer) - 1: 最大读取字节数（留1字节给字符串终止符）
         *   pdMS_TO_TICKS(500): 读取超时时间（500毫秒）
         * 返回值：实际读取的字节数 */
        int len = uart_read_bytes(UART_PORT, rx_buffer, sizeof(rx_buffer) - 1, pdMS_TO_TICKS(500));
        
        if (len > 0) {
            /* 读取成功，添加字符串终止符 */
            rx_buffer[len] = '\0';
            
            /* 记录日志，显示接收到的数据 */
            ESP_LOGI(TAG, "received %d bytes: %s", len, (char *)rx_buffer);
        } else {
            /* 读取超时，没有接收到数据 */
            ESP_LOGW(TAG, "no data received; check TX/RX jumper and GPIO settings");
        }

        /* 延时2秒后继续下一次回环测试 */
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}