/**
 * @file main.c
 * @brief LVGL v9演示示例主文件
 * 
 * 本示例展示了如何在ESP32-P4上使用LVGL v9图形库进行UI开发：
 * 1. 初始化BSP显示驱动，配置三重部分防撕裂模式
 * 2. 开启LCD背光
 * 3. 运行LVGL演示（widgets演示）
 * 
 * 注意：如需使用三重缓存防撕裂配置，需要修复ESP-IDF 5.5版本的相关问题。
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"
#include "lv_demos.h"

/**
 * @brief ESP-IDF应用程序入口函数
 * 
 * 主函数负责初始化LVGL图形库和显示系统：
 * 1. 配置显示参数，包括LVGL适配器配置、屏幕旋转和防撕裂模式
 * 2. 启动BSP显示
 * 3. 开启LCD背光
 * 4. 锁定显示并运行LVGL演示
 * 
 * @return void 无返回值
 */
void app_main(void)
{
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0
        }};
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    bsp_display_lock(-1);

    // lv_demo_music();
    // lv_demo_benchmark();
    lv_demo_widgets();

    bsp_display_unlock();
}
