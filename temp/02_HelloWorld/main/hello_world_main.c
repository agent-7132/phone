/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * @file hello_world_main.c
 * @brief ESP-IDF HelloWorld示例程序主入口文件
 * 
 * 该示例程序展示了ESP-IDF基础框架的使用方法，包括：
 * 1. 系统初始化与应用程序入口
 * 2. 芯片信息获取与打印（CPU核数、功能特性、硅版本）
 * 3. Flash容量检测与类型识别
 * 4. 系统堆内存信息查询
 * 5. FreeRTOS任务延时与系统重启
 * 
 * @note 该程序运行后会打印Hello World信息、芯片详细信息、Flash信息和堆内存信息，
 *       然后等待10秒后自动重启系统。
 */

#include <stdio.h>              /* 标准输入输出库，用于printf函数 */
#include <inttypes.h>           /* 标准整数类型格式宏，用于PRIu32等格式化输出 */
#include "sdkconfig.h"          /* ESP-IDF配置头文件，包含编译时配置选项（如CONFIG_IDF_TARGET） */
#include "freertos/FreeRTOS.h"  /* FreeRTOS实时操作系统核心头文件 */
#include "freertos/task.h"      /* FreeRTOS任务管理头文件，用于vTaskDelay函数 */
#include "esp_chip_info.h"      /* ESP芯片信息查询接口，用于获取芯片型号和特性 */
#include "esp_flash.h"          /* ESP Flash操作接口，用于获取Flash容量信息 */
#include "esp_system.h"         /* ESP系统级操作接口，用于系统重启等功能 */

/**
 * @brief ESP-IDF应用程序入口函数
 * 
 * 该函数是ESP-IDF应用程序的主入口点，在系统初始化完成后由框架自动调用。
 * 函数执行流程：
 * 1. 打印Hello World欢迎信息
 * 2. 获取并打印芯片详细信息（型号、CPU核数、功能特性、硅版本）
 * 3. 获取并打印Flash容量与类型信息
 * 4. 打印系统最小空闲堆内存大小
 * 5. 启动10秒倒计时，每秒打印剩余时间
 * 6. 倒计时结束后执行系统重启
 * 
 * @return 该函数通常不返回（会执行系统重启），若获取Flash大小失败则提前返回
 */
void app_main(void)
{
    /* 打印Hello World欢迎信息到串口终端 */
    printf("Hello world!\n");

    /* 定义芯片信息结构体变量，用于存储芯片查询结果 */
    esp_chip_info_t chip_info;
    /* 定义Flash容量变量，单位为字节 */
    uint32_t flash_size;
    
    /* 调用芯片信息查询函数，获取当前ESP芯片的详细信息 */
    esp_chip_info(&chip_info);
    
    /* 打印芯片型号、CPU核数及功能特性信息 */
    /* CONFIG_IDF_TARGET: 编译目标芯片型号（如esp32、esp32s3等） */
    /* chip_info.cores: CPU核心数量 */
    /* CHIP_FEATURE_WIFI_BGN: WiFi 802.11b/g/n功能特性标志 */
    /* CHIP_FEATURE_BT: 蓝牙Classic功能特性标志 */
    /* CHIP_FEATURE_BLE: 蓝牙低功耗(BLE)功能特性标志 */
    /* CHIP_FEATURE_IEEE802154: IEEE 802.15.4协议(Zigbee/Thread)功能特性标志 */
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    /* 将芯片版本号分解为主版本号和次版本号 */
    /* chip_info.revision: 芯片硅版本号，格式为MMmm（MM为主版本，mm为次版本） */
    unsigned major_rev = chip_info.revision / 100;  /* 提取主版本号 */
    unsigned minor_rev = chip_info.revision % 100;  /* 提取次版本号 */
    
    /* 打印芯片硅版本信息 */
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    
    /* 调用Flash容量查询函数，获取连接到芯片的Flash芯片容量 */
    /* 参数说明：
     *   NULL: Flash芯片索引，NULL表示默认芯片（SPI0/QSPI0）
     *   &flash_size: 输出参数，用于存储查询到的Flash容量（单位：字节）
     * 返回值：ESP_OK表示查询成功，其他值表示查询失败 */
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        /* 查询失败时打印错误信息并提前返回 */
        printf("Get flash size failed");
        return;
    }

    /* 打印Flash容量和类型信息 */
    /* flash_size / (1024 * 1024): 将字节转换为兆字节(MB) */
    /* CHIP_FEATURE_EMB_FLASH: 嵌入式Flash特性标志，若置位表示使用片上集成Flash */
    /* PRIu32: uint32_t类型的格式化输出宏，确保跨平台兼容性 */
    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    /* 打印系统启动以来的最小空闲堆内存大小 */
    /* esp_get_minimum_free_heap_size(): 获取系统运行过程中曾经达到的最小空闲堆内存 */
    /* 该值反映了系统内存使用的峰值情况，可用于内存优化参考 */
    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    /* 启动10秒倒计时循环，每秒打印剩余时间 */
    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        
        /* 调用FreeRTOS任务延时函数，延时1秒 */
        /* portTICK_PERIOD_MS: FreeRTOS系统节拍周期（毫秒），通常为1ms */
        /* 1000 / portTICK_PERIOD_MS = 1000个系统节拍，即1秒 */
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    
    /* 打印重启提示信息 */
    printf("Restarting now.\n");
    
    /* 刷新标准输出缓冲区，确保所有打印信息在重启前输出完成 */
    fflush(stdout);
    
    /* 调用系统重启函数，执行软重启 */
    /* 重启后系统将重新执行启动流程，再次进入app_main函数 */
    esp_restart();
}