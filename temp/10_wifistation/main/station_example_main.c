/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file station_example_main.c
 * @brief WiFi Station模式示例程序
 * 
 * 本示例演示了ESP32-P4如何以Station模式连接到WiFi网络：
 * 1. 初始化NVS Flash存储
 * 2. 初始化网络接口和事件循环
 * 3. 创建WiFi Station接口
 * 4. 配置WiFi连接参数（SSID、密码、认证模式）
 * 5. 注册WiFi和IP事件处理函数
 * 6. 启动WiFi并连接到指定AP
 * 7. 处理连接成功/失败事件
 * 
 * 使用方法：
 * 1. 通过idf.py menuconfig配置WiFi SSID和密码
 * 2. 将程序烧录到开发板
 * 3. 打开串口监视器查看连接状态
 * 
 * 关键技术点：
 * - 使用FreeRTOS事件组管理连接状态
 * - 通过事件回调处理WiFi连接和IP获取事件
 * - 支持自动重连机制（可配置最大重试次数）
 */

/* 标准库头文件 */
#include <string.h>             /* 字符串处理函数 */

/* FreeRTOS相关头文件 */
#include "freertos/FreeRTOS.h"  /* FreeRTOS实时操作系统核心API */
#include "freertos/task.h"      /* FreeRTOS任务管理API */
#include "freertos/event_groups.h" /* FreeRTOS事件组API */

/* ESP-IDF核心组件头文件 */
#include "esp_system.h"         /* ESP系统级API */
#include "esp_wifi.h"           /* WiFi驱动API */
#include "esp_event.h"          /* ESP事件循环API */
#include "esp_log.h"            /* ESP日志库 */
#include "nvs_flash.h"          /* NVS Flash存储API */

/* lwIP网络协议栈头文件 */
#include "lwip/err.h"           /* lwIP错误码定义 */
#include "lwip/sys.h"           /* lwIP系统调用封装 */

/* WiFi配置参数，从menuconfig配置中获取 */
/* 如果不想使用menuconfig，可以直接修改为字符串常量 */
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID       /* WiFi SSID */
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD   /* WiFi密码 */
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY   /* 最大重试次数 */

/* WPA3 SAE模式配置 */
#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif

/* WiFi认证模式阈值配置 */
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* FreeRTOS事件组句柄，用于信号连接状态 */
static EventGroupHandle_t s_wifi_event_group;

/* 事件组位定义 */
/* 事件组支持多个位表示不同事件，这里只关心两个事件：
 * - WIFI_CONNECTED_BIT: 已连接到AP并获取IP地址
 * - WIFI_FAIL_BIT: 达到最大重试次数后连接失败 */
#define WIFI_CONNECTED_BIT BIT0  /* 连接成功标志位 */
#define WIFI_FAIL_BIT      BIT1  /* 连接失败标志位 */

/* 日志标签，用于标识本模块的日志输出 */
static const char *TAG = "wifi station";

/* 重试计数变量，记录当前重试次数 */
static int s_retry_num = 0;

/**
 * @brief WiFi和IP事件处理函数
 * 
 * 该函数作为事件回调，处理WiFi和IP相关事件：
 * 1. WIFI_EVENT_STA_START: WiFi Station启动，开始连接AP
 * 2. WIFI_EVENT_STA_DISCONNECTED: WiFi连接断开，根据重试次数决定是否重连
 * 3. IP_EVENT_STA_GOT_IP: 获取到IP地址，设置连接成功标志
 * 
 * @param arg 事件参数（未使用）
 * @param event_base 事件基（WIFI_EVENT或IP_EVENT）
 * @param event_id 事件ID
 * @param event_data 事件数据指针
 */
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    /* 处理WiFi事件 */
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        /* WiFi Station启动成功，开始连接AP */
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        /* WiFi连接断开 */
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            /* 重试次数未达上限，重新连接 */
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            /* 达到最大重试次数，设置连接失败标志 */
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        /* 获取到IP地址 */
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        
        /* 打印获取到的IP地址 */
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        
        /* 重置重试计数 */
        s_retry_num = 0;
        
        /* 设置连接成功标志 */
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief WiFi Station模式初始化函数
 * 
 * 该函数负责初始化WiFi并连接到AP：
 * 1. 创建FreeRTOS事件组
 * 2. 初始化网络接口
 * 3. 创建默认事件循环
 * 4. 创建默认WiFi Station接口
 * 5. 初始化WiFi驱动
 * 6. 注册WiFi和IP事件处理函数
 * 7. 配置WiFi连接参数
 * 8. 设置WiFi模式为Station模式
 * 9. 应用WiFi配置
 * 10. 启动WiFi
 * 11. 等待连接结果（成功或失败）
 * 
 * @return void 无返回值
 */
void wifi_init_sta(void)
{
    /* 创建FreeRTOS事件组，用于管理WiFi连接状态 */
    s_wifi_event_group = xEventGroupCreate();

    /* 初始化TCP/IP网络栈 */
    ESP_ERROR_CHECK(esp_netif_init());

    /* 创建默认事件循环 */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    /* 创建默认WiFi Station接口 */
    esp_netif_create_default_wifi_sta();

    /* WiFi初始化配置结构体，使用默认配置 */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    
    /* 初始化WiFi驱动 */
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* 事件处理实例句柄 */
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    
    /* 注册WiFi事件处理函数（处理所有WiFi事件） */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    
    /* 注册IP事件处理函数（处理获取IP事件） */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    /* WiFi连接配置结构体 */
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,         /* WiFi SSID */
            .password = EXAMPLE_ESP_WIFI_PASS,     /* WiFi密码 */
            /* 认证模式阈值：如果密码符合WPA2标准（长度>=8），默认重置为WPA2
             * 如果要连接到废弃的WEP/WPA网络，请将阈值设置为对应的认证模式
             * 并设置符合标准的密码长度和格式 */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,  /* 认证模式阈值 */
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,     /* WPA3 SAE模式 */
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,  /* WPA3 H2E标识符 */
        },
    };
    
    /* 设置WiFi模式为Station模式 */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    
    /* 应用WiFi配置 */
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    
    /* 启动WiFi */
    ESP_ERROR_CHECK(esp_wifi_start() );

    /* 打印初始化完成信息 */
    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* 等待连接结果：
     * - WIFI_CONNECTED_BIT: 连接成功并获取IP
     * - WIFI_FAIL_BIT: 达到最大重试次数连接失败
     * 事件由event_handler()函数设置 */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,        /* 不清除已设置的位 */
            pdFALSE,        /* 不需要等待所有位都设置 */
            portMAX_DELAY); /* 无限等待 */

    /* xEventGroupWaitBits()返回调用前已设置的位，因此可以判断发生了哪个事件 */
    if (bits & WIFI_CONNECTED_BIT) {
        /* 连接成功 */
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        /* 连接失败 */
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        /* 发生未知事件 */
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

/**
 * @brief ESP-IDF应用程序入口函数
 * 
 * 主函数负责初始化系统并启动WiFi连接：
 * 1. 初始化NVS Flash存储（WiFi配置信息存储在NVS中）
 * 2. 打印WiFi模式信息
 * 3. 调用WiFi初始化函数连接到AP
 * 
 * @return void 无返回值
 */
void app_main(void)
{
    /* 初始化NVS Flash存储 */
    esp_err_t ret = nvs_flash_init();
    
    /* 处理NVS初始化失败的情况 */
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      /* NVS分区没有足够空闲页或格式版本不兼容，擦除后重新初始化 */
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    
    /* 检查NVS初始化结果 */
    ESP_ERROR_CHECK(ret);

    /* 打印WiFi模式信息 */
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    
    /* 初始化WiFi Station模式并连接到AP */
    wifi_init_sta();
}