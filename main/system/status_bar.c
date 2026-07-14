#include "status_bar.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "STATUS_BAR";

static status_bar_t s_status_bar = {0};

status_bar_t *status_bar_create(lv_obj_t *parent)
{
    s_status_bar.bar = lv_obj_create(parent);
    lv_obj_set_size(s_status_bar.bar, lv_display_get_horizontal_resolution(lv_display_get_default()), 40);
    lv_obj_set_style_bg_color(s_status_bar.bar, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(s_status_bar.bar, 0, 0);
    lv_obj_set_style_pad_all(s_status_bar.bar, 0, 0);
    lv_obj_align(s_status_bar.bar, LV_ALIGN_TOP_MID, 0, 0);

    s_status_bar.clock_label = lv_label_create(s_status_bar.bar);
    lv_label_set_text(s_status_bar.clock_label, "00:00");
    lv_obj_set_style_text_font(s_status_bar.clock_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_bar.clock_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_status_bar.clock_label, LV_ALIGN_LEFT_MID, 10, 0);

    s_status_bar.wifi_icon = lv_label_create(s_status_bar.bar);
    lv_label_set_text(s_status_bar.wifi_icon, "WiFi");
    lv_obj_set_style_text_font(s_status_bar.wifi_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_bar.wifi_icon, lv_color_hex(0x00FF00), 0);
    lv_obj_align(s_status_bar.wifi_icon, LV_ALIGN_RIGHT_MID, -70, 0);

    s_status_bar.battery_icon = lv_label_create(s_status_bar.bar);
    lv_label_set_text(s_status_bar.battery_icon, "Bat: 100%");
    lv_obj_set_style_text_font(s_status_bar.battery_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_bar.battery_icon, lv_color_hex(0xFFD700), 0);
    lv_obj_align(s_status_bar.battery_icon, LV_ALIGN_RIGHT_MID, -10, 0);

    s_status_bar.memory_label = lv_label_create(s_status_bar.bar);
    lv_label_set_text(s_status_bar.memory_label, "Mem: --");
    lv_obj_set_style_text_font(s_status_bar.memory_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_bar.memory_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(s_status_bar.memory_label, LV_ALIGN_RIGHT_MID, -140, 0);

    ESP_LOGI(TAG, "Status bar created");
    return &s_status_bar;
}

void status_bar_update_clock(void)
{
    if (!s_status_bar.clock_label) return;
    
    static int seconds = 0;
    seconds++;
    
    int hours = (seconds / 3600) % 24;
    int mins = (seconds / 60) % 60;
    int secs = seconds % 60;
    
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", hours, mins, secs);
    lv_label_set_text(s_status_bar.clock_label, time_str);
}

void status_bar_update_memory(void)
{
    if (!s_status_bar.memory_label) return;
    
    size_t free_heap = esp_get_free_heap_size();
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    
    char mem_str[32];
    snprintf(mem_str, sizeof(mem_str), "Heap:%dk PSRAM:%dk", 
             (int)(free_heap / 1024), (int)(free_psram / 1024));
    lv_label_set_text(s_status_bar.memory_label, mem_str);
}

void status_bar_set_wifi(bool connected)
{
    if (!s_status_bar.wifi_icon) return;
    
    if (connected) {
        lv_label_set_text(s_status_bar.wifi_icon, "WiFi");
        lv_obj_set_style_text_color(s_status_bar.wifi_icon, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(s_status_bar.wifi_icon, "WiFi");
        lv_obj_set_style_text_color(s_status_bar.wifi_icon, lv_color_hex(0x666666), 0);
    }
}

void status_bar_set_battery(int percent)
{
    if (!s_status_bar.battery_icon) return;
    
    char bat_str[16];
    snprintf(bat_str, sizeof(bat_str), "Bat: %d%%", percent);
    lv_label_set_text(s_status_bar.battery_icon, bat_str);
    
    if (percent > 50) {
        lv_obj_set_style_text_color(s_status_bar.battery_icon, lv_color_hex(0xFFD700), 0);
    } else if (percent > 20) {
        lv_obj_set_style_text_color(s_status_bar.battery_icon, lv_color_hex(0xFFA500), 0);
    } else {
        lv_obj_set_style_text_color(s_status_bar.battery_icon, lv_color_hex(0xFF0000), 0);
    }
}
