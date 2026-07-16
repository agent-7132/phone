#include "alarm_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "ui_animation.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "ALARM_APP";

static lv_obj_t *scr = NULL;
static lv_obj_t *alarm_list = NULL;
static lv_obj_t *status_label = NULL;

#define MAX_ALARMS 10

typedef struct {
    char name[32];
    int hour;
    int minute;
    bool enabled;
    bool repeating;
    int repeat_days;
} alarm_t;

static alarm_t alarms[MAX_ALARMS];
static int alarm_count = 0;
static TaskHandle_t alarm_task_handle = NULL;

static void back_button_cb(lv_event_t *e);
static void add_alarm(lv_event_t *e);
static void toggle_alarm(lv_event_t *e);
static void delete_alarm(lv_event_t *e);

static void update_alarm_list(void)
{
    if (!alarm_list) return;
    
    lv_obj_clean(alarm_list);
    
    for (int i = 0; i < alarm_count; i++) {
        lv_obj_t *alarm_item = lv_obj_create(alarm_list);
        lv_obj_set_size(alarm_item, 420, 70);
        lv_obj_set_style_bg_color(alarm_item, lv_color_hex(0x252540), 0);
        lv_obj_set_style_border_width(alarm_item, 0, 0);
        lv_obj_set_style_radius(alarm_item, 12, 0);
        lv_obj_set_style_shadow_width(alarm_item, 6, 0);
        lv_obj_set_style_shadow_color(alarm_item, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(alarm_item, 50, 0);
        
        lv_obj_t *time_label = lv_label_create(alarm_item);
        char time_str[16];
        snprintf(time_str, sizeof(time_str), "%02d:%02d", alarms[i].hour, alarms[i].minute);
        lv_label_set_text(time_label, time_str);
        lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(time_label, 
            alarms[i].enabled ? lv_color_hex(0x4CAF50) : lv_color_hex(0x666666), 0);
        lv_obj_align(time_label, LV_ALIGN_LEFT_MID, 20, 0);
        
        lv_obj_t *name_label = lv_label_create(alarm_item);
        lv_label_set_text(name_label, alarms[i].name[0] ? alarms[i].name : "Alarm");
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(name_label, lv_color_hex(0xCCCCCC), 0);
        lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 100, 0);
        
        lv_obj_t *toggle_btn = lv_btn_create(alarm_item);
        lv_obj_set_size(toggle_btn, 54, 32);
        lv_obj_set_style_bg_color(toggle_btn, 
            alarms[i].enabled ? lv_color_hex(0x4CAF50) : lv_color_hex(0x3a3a5a), 0);
        lv_obj_set_style_bg_color(toggle_btn, 
            alarms[i].enabled ? lv_color_hex(0x388E3C) : lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(toggle_btn, 0, 0);
        lv_obj_set_style_radius(toggle_btn, 16, 0);
        lv_obj_set_style_shadow_width(toggle_btn, 4, 0);
        lv_obj_set_style_shadow_color(toggle_btn, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(toggle_btn, 40, 0);
        lv_obj_align(toggle_btn, LV_ALIGN_RIGHT_MID, -65, 0);
        lv_obj_set_user_data(toggle_btn, (void *)(intptr_t)i);
        lv_obj_add_event_cb(toggle_btn, toggle_alarm, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t *toggle_icon = lv_label_create(toggle_btn);
        lv_label_set_text(toggle_icon, alarms[i].enabled ? "✓" : "");
        lv_obj_set_style_text_font(toggle_icon, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(toggle_icon, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(toggle_icon);
        
        lv_obj_t *delete_btn = lv_btn_create(alarm_item);
        lv_obj_set_size(delete_btn, 44, 44);
        lv_obj_set_style_bg_color(delete_btn, lv_color_hex(0xE53935), 0);
        lv_obj_set_style_bg_color(delete_btn, lv_color_hex(0xC62828), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(delete_btn, 0, 0);
        lv_obj_set_style_radius(delete_btn, 22, 0);
        lv_obj_set_style_shadow_width(delete_btn, 4, 0);
        lv_obj_set_style_shadow_color(delete_btn, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(delete_btn, 40, 0);
        lv_obj_align(delete_btn, LV_ALIGN_RIGHT_MID, -10, 0);
        lv_obj_set_user_data(delete_btn, (void *)(intptr_t)i);
        lv_obj_add_event_cb(delete_btn, delete_alarm, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t *delete_icon = lv_label_create(delete_btn);
        lv_label_set_text(delete_icon, "×");
        lv_obj_set_style_text_font(delete_icon, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(delete_icon, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(delete_icon);
    }
    
    char status[64];
    snprintf(status, sizeof(status), "Alarms: %d", alarm_count);
    lv_label_set_text(status_label, status);
}

static void alarm_check_task(void *arg)
{
    (void)arg;
    
    while (1) {
        int64_t now = esp_timer_get_time() / 1000000;
        int hours = (now / 3600) % 24;
        int minutes = (now / 60) % 60;
        
        for (int i = 0; i < alarm_count; i++) {
            if (alarms[i].enabled && 
                alarms[i].hour == hours && 
                alarms[i].minute == minutes) {
                ESP_LOGI(TAG, "Alarm triggered: %s", alarms[i].name);
                lv_label_set_text(status_label, "Alarm ringing!");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

static void add_default_alarms(void)
{
    alarm_count = 0;
    
    strcpy(alarms[alarm_count].name, "Morning");
    alarms[alarm_count].hour = 7;
    alarms[alarm_count].minute = 0;
    alarms[alarm_count].enabled = true;
    alarms[alarm_count].repeating = true;
    alarms[alarm_count].repeat_days = 0x7F;
    alarm_count++;
    
    strcpy(alarms[alarm_count].name, "Work");
    alarms[alarm_count].hour = 8;
    alarms[alarm_count].minute = 30;
    alarms[alarm_count].enabled = false;
    alarms[alarm_count].repeating = true;
    alarms[alarm_count].repeat_days = 0x1F;
    alarm_count++;
}

lv_obj_t *alarm_app_create(void)
{
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    
    status_bar_create(scr);
    
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Alarm");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);
    
    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 44, 44);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 10, 0);
    lv_obj_set_style_shadow_width(back_btn, 4, 0);
    lv_obj_set_style_shadow_color(back_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(back_btn, 40, 0);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 50);
    lv_obj_add_event_cb(back_btn, back_button_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "←");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_center(back_label);
    
    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "Loading...");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x87CEEB), 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 80);
    
    alarm_list = lv_obj_create(scr);
    lv_obj_set_size(alarm_list, 440, 450);
    lv_obj_set_style_bg_color(alarm_list, lv_color_hex(0x202038), 0);
    lv_obj_set_style_border_width(alarm_list, 0, 0);
    lv_obj_set_style_radius(alarm_list, 16, 0);
    lv_obj_set_style_shadow_width(alarm_list, 8, 0);
    lv_obj_set_style_shadow_color(alarm_list, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(alarm_list, 60, 0);
    lv_obj_align(alarm_list, LV_ALIGN_TOP_MID, 0, 110);
    
    lv_obj_t *add_btn = lv_btn_create(scr);
    lv_obj_set_size(add_btn, 440, 56);
    lv_obj_set_style_bg_color(add_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_bg_color(add_btn, lv_color_hex(0x388E3C), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(add_btn, 0, 0);
    lv_obj_set_style_radius(add_btn, 12, 0);
    lv_obj_set_style_shadow_width(add_btn, 6, 0);
    lv_obj_set_style_shadow_color(add_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(add_btn, 50, 0);
    lv_obj_align(add_btn, LV_ALIGN_TOP_MID, 0, 570);
    lv_obj_add_event_cb(add_btn, add_alarm, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *add_label = lv_label_create(add_btn);
    lv_label_set_text(add_label, "+ Add Alarm");
    lv_obj_set_style_text_font(add_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(add_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(add_label);
    
    add_default_alarms();
    update_alarm_list();
    
    if (!alarm_task_handle) {
        xTaskCreate(alarm_check_task, "alarm_check", 2048, NULL, 5, &alarm_task_handle);
    }
    
    ui_animation_slide(scr, LV_DIR_RIGHT, 300);
    
    ESP_LOGI(TAG, "Alarm app created with modern design");
    
    return scr;
}

static void back_button_cb(lv_event_t *e)
{
    (void)e;
    app_manager_go_home();
}

void alarm_app_on_exit(void)
{
    if (alarm_task_handle) {
        vTaskDelete(alarm_task_handle);
        alarm_task_handle = NULL;
    }
    scr = NULL;
    alarm_list = NULL;
    status_label = NULL;
    ESP_LOGI(TAG, "Alarm app exited and cleaned up");
}

static void add_alarm(lv_event_t *e)
{
    (void)e;
    if (alarm_count >= MAX_ALARMS) return;
    
    strcpy(alarms[alarm_count].name, "");
    alarms[alarm_count].hour = 8;
    alarms[alarm_count].minute = 0;
    alarms[alarm_count].enabled = true;
    alarms[alarm_count].repeating = false;
    alarms[alarm_count].repeat_days = 0;
    alarm_count++;
    
    update_alarm_list();
}

static void toggle_alarm(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(btn);
    
    if (index < 0 || index >= alarm_count) return;
    
    alarms[index].enabled = !alarms[index].enabled;
    update_alarm_list();
}

static void delete_alarm(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(btn);
    
    if (index < 0 || index >= alarm_count) return;
    
    for (int i = index; i < alarm_count - 1; i++) {
        alarms[i] = alarms[i + 1];
    }
    alarm_count--;
    
    update_alarm_list();
}