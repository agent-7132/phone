#include <stdio.h>
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp/touch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "system/app_manager.h"
#include "system/status_bar.h"
#include "system/home_screen.h"
#include "system/app_installer.h"
#include "apps/display_app.h"
#include "apps/touch_app.h"
#include "apps/i2c_app.h"
#include "apps/sdcard_app.h"
#include "apps/audio_app.h"
#include "apps/settings_app.h"
#include "apps/info_app.h"

static const char *TAG = "PHONE_SYSTEM";

static void status_bar_update_task(void *arg)
{
    while (1) {
        bsp_display_lock(portMAX_DELAY);
        status_bar_update_clock();
        status_bar_update_memory();
        bsp_display_unlock();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void register_native_apps(void)
{
    app_info_t app = {0};
    
    strcpy(app.name, "display");
    strcpy(app.title, "Display");
    strcpy(app.icon, "🎨");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = display_app_create;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "touch");
    strcpy(app.title, "Touch");
    strcpy(app.icon, "👆");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = touch_app_create;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "i2c");
    strcpy(app.title, "I2C Scan");
    strcpy(app.icon, "🔍");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = i2c_app_create;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "sdcard");
    strcpy(app.title, "SD Card");
    strcpy(app.icon, "💾");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = sdcard_app_create;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "audio");
    strcpy(app.title, "Audio");
    strcpy(app.icon, "🎵");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = audio_app_create;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "settings");
    strcpy(app.title, "Settings");
    strcpy(app.icon, "⚙️");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = settings_app_create;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "info");
    strcpy(app.title, "System Info");
    strcpy(app.icon, "ℹ️");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = info_app_create;
    app_manager_register(&app);
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================== ESP32-P4 PHONE SYSTEM START ====================");
    
    ESP_LOGI(TAG, "Initializing BSP...");
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE,
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        }
    };
    
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "Display init failed!");
        return;
    }
    ESP_LOGI(TAG, "BSP initialized");
    
    bsp_display_backlight_on();
    ESP_LOGI(TAG, "Backlight ON");
    
    ESP_LOGI(TAG, "Initializing SPIFFS...");
    esp_err_t ret = bsp_spiffs_mount();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS mounted");
    } else {
        ESP_LOGW(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "Initializing app manager...");
    app_manager_init();
    register_native_apps();
    
    ESP_LOGI(TAG, "Scanning for dynamic apps...");
    app_installer_init();
    app_installer_scan_and_register();
    
    ESP_LOGI(TAG, "Loading home screen...");
    app_manager_go_home();
    
    ESP_LOGI(TAG, "Creating status bar update task...");
    xTaskCreate(status_bar_update_task, "status_bar_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "==================== SYSTEM READY ====================");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
