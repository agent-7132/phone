/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file gpio_interrupt_main.c
 * @brief GPIO中断示例程序
 * 
 * 本示例演示了ESP32-P4 GPIO中断的完整实现流程：
 * 1. 配置GPIO引脚为输入模式并启用边缘中断
 * 2. 安装中断服务并注册中断处理函数
 * 3. 在中断处理函数中实现防抖逻辑
 * 4. 通过FreeRTOS队列将中断事件传递给任务处理
 * 5. 在任务中接收并打印中断事件信息
 * 
 * 使用方法：
 * 1. 通过idf.py menuconfig配置输入GPIO编号和防抖时间
 * 2. 将程序烧录到开发板
 * 3. 触发输入引脚（如按键按下），观察串口终端的中断事件输出
 * 
 * 关键技术点：
 * - 使用IRAM_ATTR将中断处理函数放入IRAM，确保中断响应速度
 * - 使用队列在ISR和任务间安全传递数据
 * - 实现软件防抖防止抖动误触发
 */

/* 标准库头文件 */
#include <inttypes.h>           /* 跨平台整数类型格式化宏，如PRIu32 */
#include <stdbool.h>            /* 布尔类型定义 */
#include <stdio.h>              /* 标准输入输出函数 */
#include <stdint.h>             /* 标准整数类型定义 */

/* ESP-IDF组件头文件 */
#include "driver/gpio.h"        /* GPIO驱动API */
#include "esp_log.h"            /* ESP日志库 */
#include "esp_timer.h"          /* ESP高精度定时器API */
#include "freertos/FreeRTOS.h"  /* FreeRTOS实时操作系统核心API */
#include "freertos/queue.h"     /* FreeRTOS队列API */
#include "freertos/task.h"      /* FreeRTOS任务管理API */
#include "sdkconfig.h"          /* 项目配置头文件，包含menuconfig配置选项 */

/**
 * @brief GPIO中断事件结构体
 * 
 * 用于在中断服务程序(ISR)和任务之间传递中断事件信息，包含：
 * - gpio_num: 触发中断的GPIO编号
 * - level: 中断触发时的GPIO电平（0=低电平，1=高电平）
 * - tick: 中断发生时的FreeRTOS系统节拍数
 */
typedef struct {
    int gpio_num;                /* 触发中断的GPIO编号 */
    int level;                   /* 中断触发时的GPIO电平 */
    TickType_t tick;             /* 中断发生时的系统节拍数 */
} gpio_event_t;

/* 日志标签，用于标识本模块的日志输出 */
static const char *TAG = "gpio_interrupt";

/* GPIO事件队列句柄，用于ISR向任务传递中断事件 */
static QueueHandle_t s_gpio_evt_queue;

/* 上一次中断事件发生的时间戳（用于防抖） */
static TickType_t s_last_event_tick;

/**
 * @brief GPIO中断服务处理函数
 * 
 * 该函数在GPIO中断触发时被调用，执行以下操作：
 * 1. 获取触发中断的GPIO编号
 * 2. 实现防抖逻辑，过滤短时间内的重复触发
 * 3. 读取当前GPIO电平
 * 4. 将中断事件通过队列发送给任务处理
 * 
 * @note 该函数使用IRAM_ATTR属性，代码将被放入IRAM中执行，
 *       确保中断响应的实时性，避免Flash访问延迟影响中断处理。
 * 
 * @param arg 中断参数，此处传入GPIO编号（通过intptr_t转换）
 */
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    /* 将参数转换为GPIO编号（arg存储的是intptr_t类型的GPIO编号） */
    const int gpio_num = (int)(intptr_t)arg;
    
    /* 获取当前系统节拍数，用于防抖判断 */
    const TickType_t now_tick = xTaskGetTickCountFromISR();
    
    /* 将配置的防抖时间（毫秒）转换为FreeRTOS节拍数 */
    const TickType_t debounce_ticks = pdMS_TO_TICKS(CONFIG_EXAMPLE_DEBOUNCE_MS);

    /* 防抖判断：如果距上次中断时间小于防抖时间，则忽略本次中断 */
    if ((now_tick - s_last_event_tick) < debounce_ticks) {
        return;
    }
    
    /* 更新上一次中断事件的时间戳 */
    s_last_event_tick = now_tick;

    /* 构建中断事件结构体 */
    gpio_event_t event = {
        .gpio_num = gpio_num,              /* 触发中断的GPIO编号 */
        .level = gpio_get_level(gpio_num), /* 读取当前GPIO电平 */
        .tick = now_tick,                  /* 中断发生时的系统节拍数 */
    };
    
    /* 将中断事件发送到队列（ISR安全版本） */
    /* 参数说明：
     *   s_gpio_evt_queue: 目标队列句柄
     *   &event: 要发送的数据指针
     *   NULL: 不等待（ISR中不能阻塞） */
    xQueueSendFromISR(s_gpio_evt_queue, &event, NULL);
}

/**
 * @brief GPIO事件处理任务
 * 
 * 该任务负责从队列中接收中断事件并处理：
 * 1. 阻塞等待队列中的中断事件
 * 2. 接收到事件后记录日志，包含GPIO编号、电平状态和时间戳
 * 3. 无限循环持续接收并处理事件
 * 
 * @param arg 任务参数（未使用）
 */
static void gpio_event_task(void *arg)
{
    /* 用于存储接收到的中断事件 */
    gpio_event_t event;

    /* 无限循环，持续接收并处理中断事件 */
    while (true) {
        /* 从队列中接收中断事件（阻塞等待直到有事件） */
        /* 参数说明：
         *   s_gpio_evt_queue: 目标队列句柄
         *   &event: 接收数据的缓冲区指针
         *   portMAX_DELAY: 无限等待直到有消息可用 */
        if (xQueueReceive(s_gpio_evt_queue, &event, portMAX_DELAY) == pdPASS) {
            /* 记录中断事件日志 */
            ESP_LOGI(TAG, "gpio=%d level=%d time=%" PRIu32 " ms",
                     event.gpio_num, 
                     event.level, 
                     event.tick * portTICK_PERIOD_MS);  /* 将节拍数转换为毫秒 */
        }
    }
}

/**
 * @brief ESP-IDF应用程序入口函数
 * 
 * 主函数负责初始化GPIO中断和创建事件处理任务：
 * 1. 打印示例标题和配置信息
 * 2. 创建GPIO事件队列
 * 3. 配置输入GPIO（输入模式、可选上拉、双边沿中断）
 * 4. 安装GPIO中断服务
 * 5. 注册GPIO中断处理函数
 * 6. 创建GPIO事件处理任务
 * 
 * @return void 无返回值
 */
void app_main(void)
{
    /* 从menuconfig配置中获取输入GPIO编号 */
    const gpio_num_t input_gpio = CONFIG_EXAMPLE_GPIO_INPUT;

    /* 打印示例标题和配置信息 */
    printf("\n");
    printf("========================================\n");
    printf(" GPIO Interrupt Demo\n");
    printf("========================================\n");
    printf("Input GPIO: %d\n", input_gpio);
    printf("Debounce: %d ms\n", CONFIG_EXAMPLE_DEBOUNCE_MS);
    printf("Trigger the input pin and watch interrupt events in the monitor.\n");
    printf("\n");

    /* 创建GPIO事件队列 */
    /* 参数说明：
     *   10: 队列容量，最多可存储10个中断事件
     *   sizeof(gpio_event_t): 每条消息的大小 */
    s_gpio_evt_queue = xQueueCreate(10, sizeof(gpio_event_t));
    if (s_gpio_evt_queue == NULL) {
        /* 创建失败，记录错误日志并返回 */
        ESP_LOGE(TAG, "failed to create GPIO event queue");
        return;
    }

    /* 输入GPIO配置结构体 */
    gpio_config_t input_config = {
        .pin_bit_mask = 1ULL << input_gpio,    /* GPIO引脚位掩码，设置指定GPIO */
        .mode = GPIO_MODE_INPUT,               /* 模式：输入模式 */
        .pull_up_en = CONFIG_EXAMPLE_GPIO_INPUT_PULLUP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,  /* 根据配置决定是否启用上拉 */
        .pull_down_en = GPIO_PULLDOWN_DISABLE, /* 禁用下拉电阻 */
        .intr_type = GPIO_INTR_ANYEDGE,        /* 中断类型：双边沿触发（上升沿和下降沿都触发） */
    };
    
    /* 应用输入GPIO配置 */
    ESP_ERROR_CHECK(gpio_config(&input_config));
    
    /* 安装GPIO中断服务 */
    /* 参数说明：
     *   0: 中断服务优先级（使用默认优先级） */
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    
    /* 注册GPIO中断处理函数 */
    /* 参数说明：
     *   input_gpio: GPIO编号
     *   gpio_isr_handler: 中断处理函数指针
     *   (void *)(intptr_t)input_gpio: 传递给中断处理函数的参数 */
    ESP_ERROR_CHECK(gpio_isr_handler_add(input_gpio, gpio_isr_handler, (void *)(intptr_t)input_gpio));

    /* 创建GPIO事件处理任务 */
    /* 参数说明：
     *   gpio_event_task: 任务函数指针
     *   "gpio_event_task": 任务名称（用于调试）
     *   3072: 任务堆栈大小（字节）
     *   NULL: 任务参数
     *   10: 任务优先级（数值越大优先级越高）
     *   NULL: 任务句柄（不需要保存） */
    xTaskCreate(gpio_event_task, "gpio_event_task", 3072, NULL, 10, NULL);
}