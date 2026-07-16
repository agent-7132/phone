/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * @file nvs_counter_main.c
 * @brief NVS（Non-Volatile Storage）计数器示例主程序
 * 
 * 本示例演示了如何使用 ESP-IDF 的 NVS 组件实现一个掉电保存的启动计数器。
 * 每次设备启动时，计数器会自动加1并保存到 NVS 中，即使设备掉电后再次启动，
 * 计数器的值也会从上次保存的位置继续递增。
 */

/* 标准库头文件 */
#include <inttypes.h>           /* 跨平台整数类型格式化宏，如 PRIu32 */
#include <stdbool.h>            /* 布尔类型定义 */
#include <stdio.h>              /* 标准输入输出函数 */

/* ESP-IDF 组件头文件 */
#include "esp_err.h"            /* ESP错误码定义 */
#include "esp_log.h"            /* ESP日志记录功能 */
#include "freertos/FreeRTOS.h"  /* FreeRTOS实时操作系统核心API */
#include "freertos/task.h"      /* FreeRTOS任务管理API */
#include "nvs.h"                /* NVS非易失性存储API */
#include "nvs_flash.h"          /* NVS Flash存储初始化API */

/*
 * @def TAG
 * @brief 日志标签，用于区分不同模块的日志输出
 */
static const char *TAG = "nvs_counter";

/*
 * @def NVS_NAMESPACE
 * @brief NVS命名空间名称，用于隔离不同应用的数据
 * 
 * NVS命名空间类似于文件系统中的目录，不同命名空间下的键名可以重复，
 * 避免不同应用之间的数据冲突。
 */
static const char *NVS_NAMESPACE = "demo";

/*
 * @def NVS_KEY_BOOT_COUNT
 * @brief 启动计数值在NVS中的存储键名
 * 
 * NVS采用键值对方式存储数据，此宏定义了启动计数器对应的键名，
 * 通过该键名可以读取和写入计数值。
 */
static const char *NVS_KEY_BOOT_COUNT = "boot_count";

/*
 * @brief 初始化NVS Flash存储
 * 
 * 该函数负责初始化 ESP32 的 NVS Flash 存储系统。
 * 如果初始化失败（如Flash中没有足够的空闲页或检测到新版本格式），
 * 会先擦除整个NVS分区，然后重新初始化。
 * 
 * @note NVS初始化失败通常发生在以下情况：
 *       1. ESP_ERR_NVS_NO_FREE_PAGES：NVS分区没有足够的空闲页
 *       2. ESP_ERR_NVS_NEW_VERSION_FOUND：NVS格式版本不兼容
 * 
 * @return void 无返回值，初始化失败时会通过 ESP_ERROR_CHECK 触发断言
 */
static void init_nvs(void)
{
    /* 调用 nvs_flash_init() 初始化 NVS Flash 存储 */
    esp_err_t err = nvs_flash_init();
    
    /* 检查是否需要擦除后重新初始化 */
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* 擦除整个NVS分区，清除所有存储的数据 */
        ESP_ERROR_CHECK(nvs_flash_erase());
        
        /* 重新初始化 NVS Flash */
        err = nvs_flash_init();
    }
    
    /* 检查最终初始化结果，失败则触发断言并终止程序 */
    ESP_ERROR_CHECK(err);
}

/*
 * @brief 应用程序入口函数
 * 
 * ESP-IDF 应用程序的主入口点，相当于标准C程序的 main() 函数。
 * 本函数实现了完整的 NVS 计数器流程：
 * 1. 初始化 NVS 存储
 * 2. 打开指定命名空间
 * 3. 读取当前启动计数值
 * 4. 计数值加1
 * 5. 将新值写回 NVS
 * 6. 提交更改并关闭 NVS 句柄
 * 7. 打印计数器信息并进入无限循环
 * 
 * @return void 无返回值，函数会一直运行在无限循环中
 */
void app_main(void)
{
    /* NVS操作句柄，用于后续的读写操作 */
    nvs_handle_t nvs_handle;
    
    /* 启动计数器变量，初始化为0 */
    uint32_t boot_count = 0;

    /* 初始化 NVS Flash 存储系统 */
    init_nvs();

    /*
     * 打开指定的 NVS 命名空间
     * 参数说明：
     *   NVS_NAMESPACE：要打开的命名空间名称
     *   NVS_READWRITE：打开模式为读写模式（支持读和写操作）
     *   &nvs_handle：输出参数，返回打开后的 NVS 句柄
     */
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle));

    /*
     * 从 NVS 中读取启动计数值
     * 参数说明：
     *   nvs_handle：NVS操作句柄
     *   NVS_KEY_BOOT_COUNT：要读取的键名
     *   &boot_count：输出参数，用于存储读取到的计数值
     */
    esp_err_t err = nvs_get_u32(nvs_handle, NVS_KEY_BOOT_COUNT, &boot_count);
    
    /* 检查读取结果 */
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        /* 键不存在，表示首次运行或NVS已被擦除 */
        ESP_LOGI(TAG, "No saved boot count yet. Starting from zero.");
    } else {
        /* 读取成功或其他错误，使用 ESP_ERROR_CHECK 检查错误 */
        ESP_ERROR_CHECK(err);
    }

    /* 启动计数值加1 */
    boot_count++;
    
    /*
     * 将新的计数值写入 NVS
     * 参数说明：
     *   nvs_handle：NVS操作句柄
     *   NVS_KEY_BOOT_COUNT：要写入的键名
     *   boot_count：要写入的计数值
     */
    ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, NVS_KEY_BOOT_COUNT, boot_count));
    
    /*
     * 提交 NVS 写入操作
     * 
     * @note nvs_set_u32() 只是将数据写入 RAM 缓存，必须调用 nvs_commit()
     *       才能将数据真正写入 Flash 存储，否则掉电后数据会丢失。
     */
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    
    /*
     * 关闭 NVS 句柄
     * 
     * @note 使用完 NVS 后必须调用 nvs_close() 释放资源，避免内存泄漏。
     */
    nvs_close(nvs_handle);

    /* 在串口终端打印计数器信息 */
    printf("\n");
    printf("========================================\n");
    printf(" NVS Counter Demo\n");
    printf("========================================\n");
    printf("Saved boot count: %" PRIu32 "\n", boot_count);
    printf("Press reset. The number should increase after each boot.\n");
    printf("Erase flash if you want to reset the counter.\n");
    printf("\n");

    /*
     * 无限循环，每隔5秒打印一次当前计数值
     * 
     * 使用 vTaskDelay() 实现延时，这是 FreeRTOS 推荐的延时方式，
     * 在延时期间任务会进入阻塞状态，释放 CPU 资源供其他任务使用。
     */
    while (true) {
        /* 打印当前启动计数值到日志 */
        ESP_LOGI(TAG, "running: boot_count=%" PRIu32, boot_count);
        
        /* 延时5秒（5000毫秒），使用 pdMS_TO_TICKS() 将毫秒转换为 FreeRTOS 时钟节拍数 */
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
