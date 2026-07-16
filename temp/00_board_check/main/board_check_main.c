/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file board_check_main.c
 * @brief ESP32-P4平台板级检查示例主程序
 * 
 * 本示例用于验证ESP32-P4开发板的基本硬件功能，包括：
 * - 芯片信息检测（CPU核心数、硅版本、功能特性）
 * - Flash容量读取
 * - PSRAM状态检测与内存统计
 * - 内部堆内存统计
 * - 定时存活日志输出
 * 
 * 使用方法：
 * 1. 使用idf.py flash将程序烧录到开发板
 * 2. 打开串口监视器查看输出信息
 * 3. 根据输出确认板级硬件状态正常
 */

/* 标准C库头文件 */
#include <inttypes.h>         /* 用于跨平台整数类型格式化宏，如PRIu32 */
#include <stdbool.h>          /* 标准布尔类型定义 */
#include <stdio.h>            /* 标准输入输出函数 */

/* ESP-IDF核心组件头文件 */
#include "esp_chip_info.h"    /* ESP芯片信息查询API */
#include "esp_flash.h"        /* Flash操作API */
#include "esp_heap_caps.h"    /* 带内存特性的堆管理API */
#include "esp_log.h"          /* ESP日志库 */
#include "esp_psram.h"        /* PSRAM操作API */
#include "esp_system.h"       /* ESP系统级API */
#include "freertos/FreeRTOS.h"/* FreeRTOS实时操作系统核心头文件 */
#include "freertos/task.h"    /* FreeRTOS任务管理API */
#include "sdkconfig.h"        /* 项目配置头文件，包含编译时配置选项 */

/* 日志标签，用于标识本模块的日志输出 */
static const char *TAG = "board_check";

/**
 * @brief 打印芯片功能特性列表
 * 
 * 根据芯片特性掩码，解析并打印芯片支持的功能特性，包括：
 * Wi-Fi、蓝牙、BLE、802.15.4、嵌入式Flash、嵌入式PSRAM
 * 
 * @param features 芯片特性掩码，由esp_chip_info()函数获取
 */
static void print_chip_features(uint32_t features)
{
    /* 打印特性列表前缀 */
    printf("Features:");
    
    /* 检查并打印Wi-Fi BGN功能支持 */
    printf("%s", (features & CHIP_FEATURE_WIFI_BGN) ? " Wi-Fi" : "");
    
    /* 检查并打印经典蓝牙功能支持 */
    printf("%s", (features & CHIP_FEATURE_BT) ? " BT" : "");
    
    /* 检查并打印蓝牙低功耗(BLE)功能支持 */
    printf("%s", (features & CHIP_FEATURE_BLE) ? " BLE" : "");
    
    /* 检查并打印IEEE 802.15.4(Zigbee/Thread)功能支持 */
    printf("%s", (features & CHIP_FEATURE_IEEE802154) ? " 802.15.4" : "");
    
    /* 检查并打印嵌入式Flash功能支持 */
    printf("%s", (features & CHIP_FEATURE_EMB_FLASH) ? " embedded-flash" : "");
    
    /* 检查并打印嵌入式PSRAM功能支持 */
    printf("%s", (features & CHIP_FEATURE_EMB_PSRAM) ? " embedded-psram" : "");
    
    /* 打印换行符，结束特性列表 */
    printf("\n");
}

/**
 * @brief ESP-IDF应用程序入口函数
 * 
 * 这是ESP-IDF应用程序的主入口点，执行以下操作：
 * 1. 获取芯片信息（CPU核心数、硅版本、功能特性）
 * 2. 读取并打印Flash容量
 * 3. 检测PSRAM状态并打印可用空间
 * 4. 打印内部堆内存统计信息
 * 5. 进入循环，每5秒输出一次内存存活日志
 */
void app_main(void)
{
    /* 芯片信息结构体，用于存储芯片详细信息 */
    esp_chip_info_t chip_info;
    
    /* Flash容量变量，单位为字节 */
    uint32_t flash_size = 0;

    /* 获取芯片信息，填充chip_info结构体 */
    esp_chip_info(&chip_info);

    /* 打印板级检查标题头 */
    printf("\n");
    printf("========================================\n");
    printf(" ESP32-P4 Platform Board Check\n");
    printf("========================================\n");
    
    /* 打印IDF目标芯片名称（从sdkconfig配置获取） */
    printf("IDF target: %s\n", CONFIG_IDF_TARGET);
    
    /* 打印CPU核心数量 */
    printf("CPU cores: %d\n", chip_info.cores);
    
    /* 打印硅版本号，格式为vX.Y */
    printf("Silicon revision: v%d.%d\n", chip_info.revision / 100, chip_info.revision % 100);
    
    /* 调用函数打印芯片功能特性列表 */
    print_chip_features(chip_info.features);

    /* 读取Flash容量 */
    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        /* 转换为MB单位并打印，使用PRIu32确保跨平台兼容性 */
        printf("Flash size: %" PRIu32 " MB\n", flash_size / (1024U * 1024U));
    } else {
        /* 读取失败时输出警告日志 */
        ESP_LOGW(TAG, "Failed to read flash size");
    }

    /* 检查PSRAM是否已初始化 */
    if (esp_psram_is_initialized()) {
        /* PSRAM已初始化，打印状态和可用空间 */
        printf("PSRAM: initialized\n");
        printf("PSRAM free: %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    } else {
        /* PSRAM未初始化或未启用 */
        printf("PSRAM: not initialized or not enabled\n");
    }

    /* 打印内部堆可用空间 */
    printf("Internal heap free: %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    
    /* 打印系统启动以来的最小可用堆空间（用于检测内存泄漏） */
    printf("Minimum free heap: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
    
    /* 打印操作提示信息 */
    printf("\n");
    printf("Board check is running. Open the serial monitor and confirm this output.\n");
    printf("Next steps:\n");
    printf("  1. Run 02_HelloWorld to verify a classic ESP-IDF example.\n");
    printf("  2. Run 08_i2c_tools if you want to scan the I2C bus.\n");
    printf("  3. Choose Wi-Fi, Ethernet, SD card, display, audio, or camera examples based on your board.\n");
    printf("\n");

    /* 无限循环，定期输出内存状态日志 */
    while (true) {
        /* 输出存活日志，包含内部堆和PSRAM可用空间 */
        ESP_LOGI(TAG, "alive: free internal heap=%u bytes, free psram=%u bytes",
                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        
        /* 延迟5秒（转换为FreeRTOS ticks） */
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
