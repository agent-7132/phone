/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * @file gpio_io_main.c
 * @brief GPIO输入输出示例程序
 * 
 * 本示例演示了ESP32-P4 GPIO的基本输入输出操作：
 * 1. 配置一个GPIO引脚作为输出模式
 * 2. 配置一个GPIO引脚作为输入模式（可选上拉）
 * 3. 循环切换输出引脚电平
 * 4. 读取输入引脚电平并打印到日志
 * 
 * 使用方法：
 * 1. 通过idf.py menuconfig配置输出GPIO和输入GPIO编号
 * 2. 将程序烧录到开发板
 * 3. 观察日志输出，或用杜邦线连接输出GPIO到输入GPIO验证功能
 */

/* 标准库头文件 */
#include <stdbool.h>            /* 布尔类型定义 */
#include <stdio.h>              /* 标准输入输出函数 */

/* ESP-IDF组件头文件 */
#include "driver/gpio.h"        /* GPIO驱动API */
#include "esp_log.h"            /* ESP日志库 */
#include "freertos/FreeRTOS.h"  /* FreeRTOS核心API */
#include "freertos/task.h"      /* FreeRTOS任务管理API */
#include "sdkconfig.h"          /* 项目配置头文件，包含menuconfig配置选项 */

/* 日志标签，用于标识本模块的日志输出 */
static const gpio_num_t output_gpio = CONFIG_EXAMPLE_GPIO_OUTPUT;  /* 输出GPIO编号，由menuconfig配置 */
static const gpio_num_t input_gpio = CONFIG_EXAMPLE_GPIO_INPUT;    /* 输入GPIO编号，由menuconfig配置 */

/**
 * @brief ESP-IDF应用程序入口函数
 * 
 * 主函数负责初始化GPIO并执行输入输出操作：
 * 1. 打印示例标题和配置信息
 * 2. 配置输出GPIO（推挽输出，无上拉下拉）
 * 3. 配置输入GPIO（输入模式，可选上拉）
 * 4. 循环切换输出电平，读取输入电平并记录日志
 * 
 * @return void 无返回值
 */
void app_main(void)
{
    /* 打印示例标题和配置信息 */
    printf("\n");
    printf("========================================\n");
    printf(" GPIO IO Demo\n");
    printf("========================================\n");
    printf("Output GPIO: %d\n", output_gpio);
    printf("Input GPIO: %d\n", input_gpio);
    printf("Optional test: connect output GPIO to input GPIO and watch the input follow.\n");
    printf("\n");

    /* 输出GPIO配置结构体 */
    gpio_config_t output_config = {
        .pin_bit_mask = 1ULL << output_gpio,  /* GPIO引脚位掩码，设置指定GPIO */
        .mode = GPIO_MODE_OUTPUT,              /* 模式：输出模式 */
        .pull_up_en = GPIO_PULLUP_DISABLE,     /* 禁用上拉电阻 */
        .pull_down_en = GPIO_PULLDOWN_DISABLE, /* 禁用下拉电阻 */
        .intr_type = GPIO_INTR_DISABLE,        /* 禁用中断 */
    };
    /* 应用输出GPIO配置 */
    ESP_ERROR_CHECK(gpio_config(&output_config));

    /* 输入GPIO配置结构体 */
    gpio_config_t input_config = {
        .pin_bit_mask = 1ULL << input_gpio,    /* GPIO引脚位掩码，设置指定GPIO */
        .mode = GPIO_MODE_INPUT,               /* 模式：输入模式 */
        .pull_up_en = CONFIG_EXAMPLE_GPIO_INPUT_PULLUP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,  /* 根据配置决定是否启用上拉 */
        .pull_down_en = GPIO_PULLDOWN_DISABLE, /* 禁用下拉电阻 */
        .intr_type = GPIO_INTR_DISABLE,        /* 禁用中断 */
    };
    /* 应用输入GPIO配置 */
    ESP_ERROR_CHECK(gpio_config(&input_config));

    /* 输出电平变量，初始为低电平 */
    bool level = false;

    /* 无限循环，持续执行GPIO操作 */
    while (true) {
        /* 翻转输出电平（低->高，高->低） */
        level = !level;
        
        /* 设置输出GPIO电平 */
        /* 参数说明：
         *   output_gpio: GPIO编号
         *   level: 电平值（0=低电平，1=高电平） */
        ESP_ERROR_CHECK(gpio_set_level(output_gpio, level));

        /* 读取输入GPIO电平 */
        /* 参数说明：
         *   input_gpio: GPIO编号
         * 返回值：0=低电平，1=高电平 */
        int input_level = gpio_get_level(input_gpio);
        
        /* 记录日志，显示输出和输入电平 */
        ESP_LOGI(TAG, "output=%d input=%d", level, input_level);

        /* 延时1秒后继续循环 */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
