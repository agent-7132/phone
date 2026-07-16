/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * @file freertos_tasks_main.c
 * @brief FreeRTOS 任务与队列通信示例
 * 
 * 本示例演示了 FreeRTOS 中任务间通过消息队列进行通信的基本用法：
 * 1. 创建一个消息队列用于任务间数据传递
 * 2. 创建生产者任务，定期向队列发送消息
 * 3. 创建消费者任务，从队列中接收并处理消息
 * 
 * 通过该示例可以学习到：
 * - FreeRTOS 队列的创建和使用
 * - 任务的创建和优先级管理
 * - 任务间的同步与通信机制
 */

/* 标准库头文件 */
#include <inttypes.h>           /* 跨平台整数类型格式化宏 */
#include <stdbool.h>            /* 布尔类型定义 */
#include <stdio.h>              /* 标准输入输出函数 */

/* ESP-IDF 组件头文件 */
#include "esp_log.h"            /* ESP日志库 */
#include "esp_timer.h"          /* ESP高精度定时器API */
#include "freertos/FreeRTOS.h"  /* FreeRTOS核心API */
#include "freertos/queue.h"     /* FreeRTOS队列API */
#include "freertos/task.h"      /* FreeRTOS任务管理API */

/**
 * @brief 消息结构体定义
 * 
 * 用于在生产者和消费者任务之间传递的数据结构，包含：
 * - sequence: 消息序列号，用于标识消息顺序
 * - uptime_ms: 发送消息时的系统运行时间（毫秒）
 */
typedef struct {
    uint32_t sequence;          /* 消息序列号，从0开始递增 */
    int64_t uptime_ms;          /* 系统运行时间，单位毫秒 */
} demo_message_t;

/* 日志标签，用于标识本模块的日志输出 */
static const char *TAG = "freertos_tasks";

/* 消息队列句柄，用于生产者和消费者任务间的通信 */
static QueueHandle_t s_demo_queue;

/**
 * @brief 生产者任务函数
 * 
 * 该任务负责生成消息并发送到队列中：
 * 1. 构建包含序列号和系统时间的消息结构体
 * 2. 尝试将消息发送到队列
 * 3. 成功发送则记录日志，队列满则记录警告
 * 4. 延时1秒后重复执行
 * 
 * @param arg 任务参数（未使用）
 */
static void producer_task(void *arg)
{
    /* 消息序列号，初始化为0 */
    uint32_t sequence = 0;

    /* 无限循环，持续生产消息 */
    while (true) {
        /* 构建消息结构体 */
        demo_message_t msg = {
            .sequence = sequence++,        /* 序列号自增 */
            .uptime_ms = esp_timer_get_time() / 1000,  /* 获取系统运行时间（毫秒） */
        };

        /* 将消息发送到队列 */
        /* 参数说明：
         *   s_demo_queue: 目标队列句柄
         *   &msg: 要发送的数据指针
         *   pdMS_TO_TICKS(100): 等待超时时间，100毫秒
         * 返回值：pdPASS表示发送成功，errQUEUE_FULL表示队列已满 */
        if (xQueueSend(s_demo_queue, &msg, pdMS_TO_TICKS(100)) == pdPASS) {
            /* 发送成功，记录日志 */
            ESP_LOGI(TAG, "producer sent message %" PRIu32, msg.sequence);
        } else {
            /* 队列已满，记录警告日志 */
            ESP_LOGW(TAG, "producer queue full");
        }

        /* 延时1秒后继续生产下一条消息 */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief 消费者任务函数
 * 
 * 该任务负责从队列中接收消息并处理：
 * 1. 阻塞等待队列中的消息
 * 2. 接收到消息后记录日志
 * 3. 无限循环持续接收
 * 
 * @param arg 任务参数（未使用）
 */
static void consumer_task(void *arg)
{
    /* 用于存储接收到的消息 */
    demo_message_t msg;

    /* 无限循环，持续消费消息 */
    while (true) {
        /* 从队列中接收消息 */
        /* 参数说明：
         *   s_demo_queue: 目标队列句柄
         *   &msg: 接收数据的缓冲区指针
         *   portMAX_DELAY: 无限等待直到有消息可用
         * 返回值：pdPASS表示接收成功 */
        if (xQueueReceive(s_demo_queue, &msg, portMAX_DELAY) == pdPASS) {
            /* 接收成功，记录消息内容 */
            ESP_LOGI(TAG, "consumer received message %" PRIu32 " at %" PRId64 " ms",
                     msg.sequence, msg.uptime_ms);
        }
    }
}

/**
 * @brief ESP-IDF应用程序入口函数
 * 
 * 主函数负责初始化队列和创建任务：
 * 1. 打印示例说明信息
 * 2. 创建消息队列（容量为5，每条消息大小为demo_message_t）
 * 3. 创建生产者任务（优先级5，堆栈3072字节）
 * 4. 创建消费者任务（优先级5，堆栈3072字节）
 * 
 * @return void 无返回值
 */
void app_main(void)
{
    /* 打印示例标题和说明 */
    printf("\n");
    printf("========================================\n");
    printf(" FreeRTOS Tasks Demo\n");
    printf("========================================\n");
    printf("This demo creates a producer task, a consumer task, and a queue.\n");
    printf("Watch the serial monitor to see messages move between tasks.\n");
    printf("\n");

    /* 创建消息队列 */
    /* 参数说明：
     *   5: 队列容量，最多可存储5条消息
     *   sizeof(demo_message_t): 每条消息的大小
     * 返回值：队列句柄，失败返回NULL */
    s_demo_queue = xQueueCreate(5, sizeof(demo_message_t));
    if (s_demo_queue == NULL) {
        /* 创建失败，记录错误日志并返回 */
        ESP_LOGE(TAG, "failed to create queue");
        return;
    }

    /* 创建生产者任务 */
    /* 参数说明：
     *   producer_task: 任务函数指针
     *   "producer": 任务名称（用于调试）
     *   3072: 任务堆栈大小（字节）
     *   NULL: 任务参数
     *   5: 任务优先级（数值越大优先级越高）
     *   NULL: 任务句柄（不需要保存） */
    xTaskCreate(producer_task, "producer", 3072, NULL, 5, NULL);
    
    /* 创建消费者任务 */
    xTaskCreate(consumer_task, "consumer", 3072, NULL, 5, NULL);
}
