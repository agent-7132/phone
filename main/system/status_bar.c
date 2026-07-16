#include "status_bar.h"
#include "notification_manager.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>

static void notification_click_cb(lv_event_t *e);

static const char *TAG = "STATUS_BAR";

static status_bar_t s_status_bar = {0};

status_bar_t *status_bar_create(lv_obj_t *parent)
{
    s_status_bar.bar = lv_obj_create(parent);
    lv_obj_set_size(s_status_bar.bar, lv_display_get_horizontal_resolution(lv_display_get_default()), 44);
    lv_obj_set_style_bg_color(s_status_bar.bar, lv_color_hex(0x1a1a2e), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_status_bar.bar, lv_color_hex(0x0f0f1a), LV_PART_SCROLLBAR);
    lv_obj_set_style_border_width(s_status_bar.bar, 0, 0);
    lv_obj_set_style_pad_all(s_status_bar.bar, 0, 0);
    lv_obj_set_style_radius(s_status_bar.bar, 0, 0);
    lv_obj_align(s_status_bar.bar, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *left_area = lv_obj_create(s_status_bar.bar);
    lv_obj_set_size(left_area, 120, 44);
    lv_obj_set_style_bg_color(left_area, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(left_area, 0, 0);
    lv_obj_set_style_border_width(left_area, 0, 0);
    lv_obj_align(left_area, LV_ALIGN_LEFT_MID, 0, 0);

    s_status_bar.clock_label = lv_label_create(left_area);
    lv_label_set_text(s_status_bar.clock_label, "00:00");
    lv_obj_set_style_text_font(s_status_bar.clock_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_bar.clock_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_status_bar.clock_label, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t *right_area = lv_obj_create(s_status_bar.bar);
    lv_obj_set_size(right_area, 280, 44);
    lv_obj_set_style_bg_color(right_area, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(right_area, 0, 0);
    lv_obj_set_style_border_width(right_area, 0, 0);
    lv_obj_align(right_area, LV_ALIGN_RIGHT_MID, 0, 0);

    s_status_bar.sensor_label = lv_label_create(right_area);
    lv_label_set_text(s_status_bar.sensor_label, "--°C");
    lv_obj_set_style_text_font(s_status_bar.sensor_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_bar.sensor_label, lv_color_hex(0x4FC3F7), 0);
    lv_obj_align(s_status_bar.sensor_label, LV_ALIGN_RIGHT_MID, -10, 0);

    s_status_bar.bt_icon = lv_label_create(right_area);
    lv_label_set_text(s_status_bar.bt_icon, "🔷");
    lv_obj_set_style_text_font(s_status_bar.bt_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_bar.bt_icon, lv_color_hex(0x666666), 0);
    lv_obj_align(s_status_bar.bt_icon, LV_ALIGN_RIGHT_MID, -45, 0);

    s_status_bar.net_icon = lv_label_create(right_area);
    lv_label_set_text(s_status_bar.net_icon, "🔌");
    lv_obj_set_style_text_font(s_status_bar.net_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_bar.net_icon, lv_color_hex(0x666666), 0);
    lv_obj_align(s_status_bar.net_icon, LV_ALIGN_RIGHT_MID, -75, 0);

    s_status_bar.memory_label = lv_label_create(right_area);
    lv_label_set_text(s_status_bar.memory_label, "Mem: --");
    lv_obj_set_style_text_font(s_status_bar.memory_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_bar.memory_label, lv_color_hex(0x8888AA), 0);
    lv_obj_align(s_status_bar.memory_label, LV_ALIGN_RIGHT_MID, -145, 0);

    s_status_bar.notification_icon = lv_label_create(right_area);
    lv_label_set_text(s_status_bar.notification_icon, "");
    lv_obj_set_style_text_font(s_status_bar.notification_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_bar.notification_icon, lv_color_hex(0xFFA500), 0);
    lv_obj_align(s_status_bar.notification_icon, LV_ALIGN_RIGHT_MID, -185, 0);
    lv_obj_add_event_cb(s_status_bar.notification_icon, notification_click_cb, LV_EVENT_CLICKED, NULL);

    ESP_LOGI(TAG, "Status bar created");
    return &s_status_bar;
}

void status_bar_update_clock(void)
{
    if (!s_status_bar.clock_label) return;
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", t->tm_hour, t->tm_min);
    lv_label_set_text(s_status_bar.clock_label, time_str);
}

void status_bar_update_memory(void)
{
    if (!s_status_bar.memory_label) return;
    
    size_t free_heap = esp_get_free_heap_size();
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    
    char mem_str[24];
    snprintf(mem_str, sizeof(mem_str), "%dk/%dk", 
             (int)(free_heap / 1024), (int)(free_psram / 1024));
    lv_label_set_text(s_status_bar.memory_label, mem_str);
}

void status_bar_set_sensor_data(float temperature, float humidity)
{
    if (!s_status_bar.sensor_label) return;
    
    char sensor_str[16];
    if (temperature > -100) {
        snprintf(sensor_str, sizeof(sensor_str), "%.0f°C", temperature);
    } else {
        snprintf(sensor_str, sizeof(sensor_str), "--°C");
    }
    lv_label_set_text(s_status_bar.sensor_label, sensor_str);
}

void status_bar_set_network(int type, bool connected)
{
    if (!s_status_bar.net_icon) return;
    
    if (connected) {
        if (type == 1) {
            lv_label_set_text(s_status_bar.net_icon, "📶");
        } else {
            lv_label_set_text(s_status_bar.net_icon, "🖧");
        }
        lv_obj_set_style_text_color(s_status_bar.net_icon, lv_color_hex(0x66BB6A), 0);
    } else {
        lv_label_set_text(s_status_bar.net_icon, "🔌");
        lv_obj_set_style_text_color(s_status_bar.net_icon, lv_color_hex(0x666666), 0);
    }
}

void status_bar_set_bluetooth(bool enabled, bool connected)
{
    if (!s_status_bar.bt_icon) return;
    
    if (enabled && connected) {
        lv_label_set_text(s_status_bar.bt_icon, "🔵");
        lv_obj_set_style_text_color(s_status_bar.bt_icon, lv_color_hex(0x4FC3F7), 0);
    } else if (enabled) {
        lv_label_set_text(s_status_bar.bt_icon, "🔷");
        lv_obj_set_style_text_color(s_status_bar.bt_icon, lv_color_hex(0x4FC3F7), 0);
    } else {
        lv_label_set_text(s_status_bar.bt_icon, "🔷");
        lv_obj_set_style_text_color(s_status_bar.bt_icon, lv_color_hex(0x666666), 0);
    }
}

void status_bar_set_notification(int count)
{
    if (!s_status_bar.notification_icon) return;
    
    if (count > 0) {
        lv_label_set_text(s_status_bar.notification_icon, "🔔");
        lv_obj_set_style_text_color(s_status_bar.notification_icon, lv_color_hex(0xFFA500), 0);
    } else {
        lv_label_set_text(s_status_bar.notification_icon, "");
    }
}

static void notification_click_cb(lv_event_t *e)
{
    notification_manager_show_notification_center(lv_scr_act());
}