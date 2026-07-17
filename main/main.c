#include <stdio.h>
#include "bsp/esp-bsp.h"
#include "esp_sntp.h"
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
#include "system/net_manager.h"
#include "system/theme_manager.h"
#include "system/ota_manager.h"
#include "system/power_manager.h"
#include "system/bt_manager.h"
#include "system/notification_manager.h"
#include "system/gesture_manager.h"
#include "system/control_center.h"
#include "system/settings_manager.h"
#include "system/security_manager.h"
#include "apps/display_app.h"
#include "apps/touch_app.h"
#include "apps/i2c_app.h"
#include "apps/sdcard_app.h"
#include "apps/audio_app.h"
#include "apps/settings_app.h"
#include "apps/info_app.h"
#include "apps/encrypt_app.h"
#include "apps/file_browser_app.h"
#include "apps/net_settings_app.h"
#include "apps/camera_app.h"
#include "apps/music_player_app.h"
#include "apps/photo_album_app.h"
#include "apps/bt_settings_app.h"
#include "apps/reader_app.h"
#include "apps/app_store_app.h"
#include "apps/video_player_app.h"
#include "apps/calculator_app.h"
#include "apps/calendar_app.h"
#include "apps/alarm_app.h"
#include "system/app_downloader.h"
#include "system/sensor_manager.h"
#include "system/i18n_manager.h"
#include "system/task_manager.h"
#include "system/crash_handler.h"
#include "system/ui_animation.h"
#include "system/ui_feedback.h"
#include "system/permission_manager.h"
#include "system/service_manager.h"
#include "system/structured_logging.h"
#include "system/watchdog_manager.h"
#include "system/memory_pool.h"
#include "system/file_cache.h"
#include "system/usb_host_manager.h"
#include "esp_heap_caps.h"
#include "esp_chip_info.h"

static const char *TAG = "PHONE_SYSTEM";

static void log_chip_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    
    ESP_LOGI(TAG, "Chip Info:");
    ESP_LOGI(TAG, "  Model: ESP32-P4");
    ESP_LOGI(TAG, "  Silicon Revision: v%d.%d", major_rev, minor_rev);
    ESP_LOGI(TAG, "  CPU Cores: %d", chip_info.cores);
    ESP_LOGI(TAG, "  Features:");
    ESP_LOGI(TAG, "    - WiFi: %s", (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "yes" : "no");
    ESP_LOGI(TAG, "    - BLE: %s", (chip_info.features & CHIP_FEATURE_BLE) ? "yes" : "no");
    ESP_LOGI(TAG, "    - IEEE802.15.4: %s", (chip_info.features & CHIP_FEATURE_IEEE802154) ? "yes" : "no");
    ESP_LOGI(TAG, "    - Embedded Flash: %s", (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "yes" : "no");
    
    if (major_rev == 1 && minor_rev == 3) {
        ESP_LOGW(TAG, "WARNING: Silicon v1.3 is EOL (End of Life), consider upgrading to v3.x");
    } else if (major_rev >= 3) {
        ESP_LOGI(TAG, "Silicon v%d.%d is supported, errata mitigation enabled", major_rev, minor_rev);
    }
}

static bool s_sntp_initialized = false;

static void net_status_callback(net_info_t *info)
{
    bsp_display_lock(portMAX_DELAY);
    if (info->state == NET_STATE_CONNECTED) {
        status_bar_set_network(info->type, true);
        
        if (!s_sntp_initialized) {
            ESP_LOGI(TAG, "WiFi connected, initializing SNTP...");
            setenv("TZ", "CST-8", 1);
            tzset();
            esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
            esp_sntp_setservername(0, "pool.ntp.org");
            esp_sntp_init();
            s_sntp_initialized = true;
            ESP_LOGI(TAG, "SNTP initialized with CST timezone");
        }
    } else {
        status_bar_set_network(info->type, false);
    }
    bsp_display_unlock();
}

static void bt_status_callback(bt_state_t state)
{
    bsp_display_lock(portMAX_DELAY);
    if (state == BT_STATE_CONNECTED) {
        status_bar_set_bluetooth(true, true);
    } else if (state == BT_STATE_IDLE || state == BT_STATE_SCANNING || state == BT_STATE_CONNECTING) {
        status_bar_set_bluetooth(true, false);
    } else {
        status_bar_set_bluetooth(false, false);
    }
    bsp_display_unlock();
}

static void gesture_callback(gesture_type_t type, lv_point_t start, lv_point_t end)
{
    bsp_display_lock(portMAX_DELAY);
    
    switch (type) {
        case GESTURE_TYPE_SWIPE_RIGHT:
            app_manager_go_home();
            break;
        case GESTURE_TYPE_SWIPE_DOWN:
            notification_manager_show_notification_center(lv_scr_act());
            break;
        case GESTURE_TYPE_SWIPE_UP:
            control_center_show(lv_scr_act());
            break;
        default:
            break;
    }
    
    bsp_display_unlock();
}

static bool alarm_service_running = false;
static esp_err_t alarm_service_start(void *arg) {
    (void)arg;
    alarm_service_running = true;
    while (alarm_service_running) {
        int64_t now = esp_timer_get_time() / 1000000;
        int hours = (now / 3600) % 24;
        int minutes = (now / 60) % 60;
        ESP_LOGD(TAG, "Alarm service checking: %02d:%02d", hours, minutes);
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
    return ESP_OK;
}
static void alarm_service_stop(void *arg) {
    (void)arg;
    alarm_service_running = false;
}

static bool sensor_service_running = false;
static esp_err_t sensor_service_start(void *arg) {
    (void)arg;
    sensor_service_running = true;
    while (sensor_service_running) {
        sensor_manager_read_all();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    return ESP_OK;
}
static void sensor_service_stop(void *arg) {
    (void)arg;
    sensor_service_running = false;
}
static bool sensor_service_health(void *arg) {
    (void)arg;
    return sensor_service_running;
}

static bool status_service_running = false;
static esp_err_t status_service_start(void *arg) {
    (void)arg;
    status_service_running = true;
    while (status_service_running) {
        bsp_display_lock(portMAX_DELAY);
        status_bar_update_clock();
        status_bar_update_memory();
        bsp_display_unlock();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return ESP_OK;
}
static void status_service_stop(void *arg) {
    (void)arg;
    status_service_running = false;
}

static void register_native_apps(void)
{
    app_info_t app = {0};
    
    strcpy(app.name, "display");
    strcpy(app.title, "Display");
    strcpy(app.icon, "🎨");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = display_app_create;
    app.data.native.on_exit = display_app_on_exit;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "touch");
    strcpy(app.title, "Touch");
    strcpy(app.icon, "👆");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = touch_app_create;
    app.data.native.on_exit = touch_app_on_exit;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "i2c");
    strcpy(app.title, "I2C Scan");
    strcpy(app.icon, "🔍");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = i2c_app_create;
    app.data.native.on_exit = i2c_app_on_exit;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "sdcard");
    strcpy(app.title, "SD Card");
    strcpy(app.icon, "💾");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = sdcard_app_create;
    app.data.native.on_exit = sdcard_app_on_exit;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "audio");
    strcpy(app.title, "Audio");
    strcpy(app.icon, "🎵");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = audio_app_create;
    app.data.native.on_exit = audio_app_on_exit;
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
    app.data.native.on_exit = info_app_on_exit;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "encrypt");
    strcpy(app.title, "Encrypt");
    strcpy(app.icon, "🔐");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = encrypt_app_create;
    app.data.native.on_exit = encrypt_app_on_exit;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "file_browser");
    strcpy(app.title, "File Browser");
    strcpy(app.icon, "📂");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = file_browser_app_create;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "network");
    strcpy(app.title, "Network");
    strcpy(app.icon, "🌐");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = net_settings_app_create;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "camera");
    strcpy(app.title, "Camera");
    strcpy(app.icon, "📷");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = camera_app_create;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "music");
    strcpy(app.title, "Music");
    strcpy(app.icon, "🎶");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = music_player_app_create;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "album");
    strcpy(app.title, "Album");
    strcpy(app.icon, "🖼️");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = photo_album_app_create;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "bluetooth");
    strcpy(app.title, "Bluetooth");
    strcpy(app.icon, "🔷");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = bt_settings_app_create;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "reader");
    strcpy(app.title, "Reader");
    strcpy(app.icon, "📚");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = reader_app_create;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "app_store");
    strcpy(app.title, "App Store");
    strcpy(app.icon, "🛒");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = app_store_app_create;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "video_player");
    strcpy(app.title, "Video");
    strcpy(app.icon, "🎬");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = video_player_app_create;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "calculator");
    strcpy(app.title, "Calculator");
    strcpy(app.icon, "🧮");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = calculator_app_create;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "calendar");
    strcpy(app.title, "Calendar");
    strcpy(app.icon, "📅");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = calendar_app_create;
    app_manager_register(&app);
    
    memset(&app, 0, sizeof(app));
    strcpy(app.name, "alarm");
    strcpy(app.title, "Alarm");
    strcpy(app.icon, "⏰");
    app.type = APP_TYPE_NATIVE;
    app.data.native.create_screen = alarm_app_create;
    app.data.native.on_exit = alarm_app_on_exit;
    app_manager_register(&app);
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================== ESP32-P4 PHONE SYSTEM START ====================");
    
    log_chip_info();
    
    ESP_LOGI(TAG, "Initializing BSP...");
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
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
    
    ESP_LOGI(TAG, "Initializing security manager...");
    esp_err_t security_ret = security_manager_init();
    if (security_ret == ESP_OK) {
        ESP_LOGI(TAG, "Security manager initialized");
    } else {
        ESP_LOGW(TAG, "Security manager init failed: %s", esp_err_to_name(security_ret));
    }
    
    ESP_LOGI(TAG, "Scanning for dynamic apps...");
    app_installer_init();
    app_installer_scan_and_register();
    
    ESP_LOGI(TAG, "Loading home screen...");
    app_manager_go_home();
    
    ESP_LOGI(TAG, "Initializing network manager...");
    esp_err_t net_ret = net_manager_init();
    if (net_ret == ESP_OK) {
        net_manager_register_callback(net_status_callback);
        ESP_LOGI(TAG, "Network manager initialized");
    } else {
        ESP_LOGW(TAG, "Network manager init failed: %s", esp_err_to_name(net_ret));
    }
    
    ESP_LOGI(TAG, "Initializing theme manager...");
    esp_err_t theme_ret = theme_manager_init();
    if (theme_ret == ESP_OK) {
        ESP_LOGI(TAG, "Theme manager initialized");
    } else {
        ESP_LOGW(TAG, "Theme manager init failed: %s", esp_err_to_name(theme_ret));
    }
    
    ESP_LOGI(TAG, "Initializing OTA manager...");
    esp_err_t ota_ret = ota_manager_init();
    if (ota_ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA manager initialized");
    } else {
        ESP_LOGW(TAG, "OTA manager init failed: %s", esp_err_to_name(ota_ret));
    }
    
    ESP_LOGI(TAG, "Initializing power manager...");
    esp_err_t power_ret = power_manager_init();
    if (power_ret == ESP_OK) {
        ESP_LOGI(TAG, "Power manager initialized");
    } else {
        ESP_LOGW(TAG, "Power manager init failed: %s", esp_err_to_name(power_ret));
    }
    
    ESP_LOGI(TAG, "Initializing notification manager...");
    esp_err_t notif_ret = notification_manager_init();
    if (notif_ret == ESP_OK) {
        ESP_LOGI(TAG, "Notification manager initialized");
    } else {
        ESP_LOGW(TAG, "Notification manager init failed: %s", esp_err_to_name(notif_ret));
    }
    
    ESP_LOGI(TAG, "Initializing UI feedback...");
    esp_err_t feedback_ret = ui_feedback_init();
    if (feedback_ret == ESP_OK) {
        ESP_LOGI(TAG, "UI feedback initialized");
    } else {
        ESP_LOGW(TAG, "UI feedback init failed: %s", esp_err_to_name(feedback_ret));
    }
    
    ESP_LOGI(TAG, "Initializing gesture manager...");
    esp_err_t gesture_ret = gesture_manager_init();
    if (gesture_ret == ESP_OK) {
        gesture_manager_register_callback(gesture_callback);
        ESP_LOGI(TAG, "Gesture manager initialized");
    } else {
        ESP_LOGW(TAG, "Gesture manager init failed: %s", esp_err_to_name(gesture_ret));
    }
    
    ESP_LOGI(TAG, "Initializing settings manager...");
    esp_err_t settings_ret = settings_manager_init();
    if (settings_ret == ESP_OK) {
        ESP_LOGI(TAG, "Settings manager initialized");
    } else {
        ESP_LOGW(TAG, "Settings manager init failed: %s", esp_err_to_name(settings_ret));
    }

    ESP_LOGI(TAG, "Initializing control center...");
    esp_err_t ctrl_ret = control_center_init();
    if (ctrl_ret == ESP_OK) {
        ESP_LOGI(TAG, "Control center initialized");
    } else {
        ESP_LOGW(TAG, "Control center init failed: %s", esp_err_to_name(ctrl_ret));
    }
    
    ESP_LOGI(TAG, "Initializing Bluetooth manager...");
    esp_err_t bt_ret = bt_manager_init();
    if (bt_ret == ESP_OK) {
        bt_manager_register_state_cb(bt_status_callback);
        ESP_LOGI(TAG, "Bluetooth manager initialized");
    } else {
        ESP_LOGW(TAG, "Bluetooth manager init failed: %s", esp_err_to_name(bt_ret));
    }
    
    ESP_LOGI(TAG, "Initializing app downloader...");
    app_downloader_init();
    ESP_LOGI(TAG, "App downloader initialized");
    
    ESP_LOGI(TAG, "Initializing sensor manager...");
    esp_err_t sensor_ret = sensor_manager_init();
    if (sensor_ret == ESP_OK) {
        ESP_LOGI(TAG, "Sensor manager initialized");
    } else {
        ESP_LOGW(TAG, "Sensor manager init failed: %s", esp_err_to_name(sensor_ret));
    }
    
    ESP_LOGI(TAG, "Initializing i18n manager...");
    esp_err_t i18n_ret = i18n_manager_init();
    if (i18n_ret == ESP_OK) {
        ESP_LOGI(TAG, "i18n manager initialized");
    } else {
        ESP_LOGW(TAG, "i18n manager init failed: %s", esp_err_to_name(i18n_ret));
    }
    
    ESP_LOGI(TAG, "Initializing task manager...");
    esp_err_t task_ret = task_manager_init();
    if (task_ret == ESP_OK) {
        ESP_LOGI(TAG, "Task manager initialized");
    } else {
        ESP_LOGW(TAG, "Task manager init failed: %s", esp_err_to_name(task_ret));
    }
    
    ESP_LOGI(TAG, "Initializing crash handler...");
    esp_err_t crash_ret = crash_handler_init();
    if (crash_ret == ESP_OK) {
        ESP_LOGI(TAG, "Crash handler initialized");
    } else {
        ESP_LOGW(TAG, "Crash handler init failed: %s", esp_err_to_name(crash_ret));
    }
    
    ESP_LOGI(TAG, "Initializing UI animation...");
    esp_err_t anim_ret = ui_animation_init();
    if (anim_ret == ESP_OK) {
        ESP_LOGI(TAG, "UI animation initialized");
    } else {
        ESP_LOGW(TAG, "UI animation init failed: %s", esp_err_to_name(anim_ret));
    }
    
    ESP_LOGI(TAG, "Initializing permission manager...");
    esp_err_t perm_ret = permission_manager_init();
    if (perm_ret == ESP_OK) {
        ESP_LOGI(TAG, "Permission manager initialized");
    } else {
        ESP_LOGW(TAG, "Permission manager init failed: %s", esp_err_to_name(perm_ret));
    }
    
    ESP_LOGI(TAG, "Initializing service manager...");
    esp_err_t svc_ret = service_manager_init();
    if (svc_ret == ESP_OK) {
        ESP_LOGI(TAG, "Service manager initialized");
        
        ESP_LOGI(TAG, "Registering background services...");
        
        service_manager_register("alarm", alarm_service_start, alarm_service_stop, 
                                NULL, NULL, NULL, SERVICE_PRIORITY_HIGH, 4096, SERVICE_CPU_CORE_1, NULL);
        service_manager_set_auto_restart("alarm", true, 3, 5000);
        service_manager_start("alarm");
        
        service_manager_register("sensor", sensor_service_start, sensor_service_stop, 
                                NULL, NULL, sensor_service_health, SERVICE_PRIORITY_MEDIUM, 4096, SERVICE_CPU_CORE_1, NULL);
        service_manager_set_auto_restart("sensor", true, 5, 3000);
        service_manager_set_health_check("sensor", sensor_service_health, 30000);
        service_manager_start("sensor");
        
        service_manager_register("status", status_service_start, status_service_stop, 
                                NULL, NULL, NULL, SERVICE_PRIORITY_LOW, 2048, SERVICE_CPU_CORE_0, NULL);
        service_manager_start("status");
        
        ESP_LOGI(TAG, "Background services registered");
        service_manager_dump_services();
    } else {
        ESP_LOGW(TAG, "Service manager init failed: %s", esp_err_to_name(svc_ret));
    }
    
    ESP_LOGI(TAG, "Initializing structured logging...");
    esp_err_t log_ret = struct_logging_init();
    if (log_ret == ESP_OK) {
        ESP_LOGI(TAG, "Structured logging initialized");
    } else {
        ESP_LOGW(TAG, "Structured logging init failed: %s", esp_err_to_name(log_ret));
    }
    
    ESP_LOGI(TAG, "Initializing watchdog manager...");
    esp_err_t wd_ret = watchdog_manager_init();
    if (wd_ret == ESP_OK) {
        esp_err_t wd_start_ret = watchdog_manager_start(30000);
        if (wd_start_ret == ESP_OK) {
            ESP_LOGI(TAG, "Watchdog manager initialized and started");
        } else {
            ESP_LOGW(TAG, "Watchdog start failed: %s", esp_err_to_name(wd_start_ret));
        }
    } else {
        ESP_LOGW(TAG, "Watchdog manager init failed: %s", esp_err_to_name(wd_ret));
    }
    
    ESP_LOGI(TAG, "Initializing memory pool manager...");
    esp_err_t mp_ret = memory_pool_init();
    if (mp_ret == ESP_OK) {
        memory_pool_create("widgets", 256, 32);
        memory_pool_create("strings", 128, 64);
        memory_pool_create("buffers", 1024, 16);
        memory_pool_create_dma("dma_buffers", 4096, 8);
        ESP_LOGI(TAG, "Memory pool manager initialized");
    } else {
        ESP_LOGW(TAG, "Memory pool manager init failed: %s", esp_err_to_name(mp_ret));
    }
    
    ESP_LOGI(TAG, "Initializing file cache...");
    esp_err_t fc_ret = file_cache_init(64 * 1024);
    if (fc_ret == ESP_OK) {
        ESP_LOGI(TAG, "File cache initialized (64KB PSRAM)");
    } else {
        ESP_LOGW(TAG, "File cache init failed: %s", esp_err_to_name(fc_ret));
    }
    
    ESP_LOGI(TAG, "Initializing USB host manager...");
    esp_err_t usb_ret = usb_host_manager_init();
    if (usb_ret == ESP_OK) {
        ESP_LOGI(TAG, "USB host manager initialized");
    } else {
        ESP_LOGW(TAG, "USB host manager init failed: %s", esp_err_to_name(usb_ret));
    }
    
    ESP_LOGI(TAG, "Checking OTA rollback status...");
    if (ota_manager_has_pending_update()) {
        ESP_LOGI(TAG, "Pending OTA update detected, marking as valid after successful boot");
        esp_err_t ota_valid_ret = ota_manager_mark_app_valid();
        if (ota_valid_ret == ESP_OK) {
            ESP_LOGI(TAG, "OTA update marked as valid");
        } else {
            ESP_LOGE(TAG, "Failed to mark OTA valid: %s", esp_err_to_name(ota_valid_ret));
        }
    }
    
    ESP_LOGI(TAG, "==================== SYSTEM READY ====================");
    
    while (1) {
        watchdog_manager_feed();
        service_manager_run_health_checks();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
