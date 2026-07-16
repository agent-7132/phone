/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file test_esp_lcd_jd9365.c
 * @brief JD9365 LCD显示颜色条测试示例
 * 
 * 本示例展示了如何使用ESP-IDF BSP初始化JD9365 LCD面板并显示硬件颜色条图案。
 * 主要功能：
 * 1. 初始化LCD显示设备（使用BSP接口）
 * 2. 开启LCD背光
 * 3. 设置LCD显示垂直颜色条测试图案
 * 
 * JD9365是一款MIPI-DSI接口的LCD控制器，支持硬件测试图案生成。
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"

static char *TAG = "jd9365_test";
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t lcd_io;

/**
 * @brief ESP-IDF应用程序入口函数
 * 
 * 主函数负责初始化LCD显示并显示颜色条测试图案：
 * 1. 调用bsp_display_new()初始化LCD面板和IO接口
 * 2. 开启LCD背光
 * 3. 设置LCD显示垂直颜色条图案（由硬件生成）
 * 
 * 如果初始化失败，打印错误信息并返回。
 * 
 * @return void 无返回值
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Initialize LCD device");
    esp_err_t ret = bsp_display_new(NULL, &panel_handle, &lcd_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display: %s", esp_err_to_name(ret));
        return;
    }
    bsp_display_backlight_on();

    ESP_LOGI(TAG, "Show color bar pattern drawn by hardware");
    esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_BAR_VERTICAL);
}
