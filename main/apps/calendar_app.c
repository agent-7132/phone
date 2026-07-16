#include "calendar_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "ui_utils.h"
#include "ui_animation.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <time.h>

static const char *TAG = "CALENDAR";

static const char *month_names[] = {"January", "February", "March", "April", "May", "June",
                                    "July", "August", "September", "October", "November", "December"};
static const char *weekday_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

static lv_obj_t *scr = NULL;
static lv_obj_t *month_label = NULL;
static lv_obj_t *calendar_container = NULL;
static lv_obj_t *today_label = NULL;
static int current_year = 2025;
static int current_month = 0;
static int today_day = 0;
static int today_month = 0;
static int today_year = 0;

static int days_in_month(int year, int month)
{
    int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 1 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
        return 29;
    }
    return days[month];
}

static int first_day_of_month(int year, int month)
{
    int a = (14 - month) / 12;
    int y = year - a;
    int m = month + 12 * a - 2;
    return (1 + y + y/4 - y/100 + y/400 + (31*m)/12) % 7;
}

static void render_calendar(void)
{
    ESP_LOGI(TAG, "Rendering calendar: %s %d", month_names[current_month], current_year);
    
    if (!calendar_container) return;
    
    lv_obj_clean(calendar_container);
    
    lv_obj_t *header_row = lv_obj_create(calendar_container);
    lv_obj_set_size(header_row, 440, 40);
    lv_obj_set_style_bg_color(header_row, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(header_row, 0, 0);
    lv_obj_set_style_border_width(header_row, 0, 0);
    
    for (int i = 0; i < 7; i++) {
        lv_obj_t *label = lv_label_create(header_row);
        lv_label_set_text(label, weekday_names[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x87CEEB), 0);
        lv_obj_set_size(label, 62, 40);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, i * 62, 0);
        lv_obj_center(label);
    }
    
    int first_day = first_day_of_month(current_year, current_month);
    int days = days_in_month(current_year, current_month);
    int day_count = 0;
    
    for (int week = 0; week < 6; week++) {
        lv_obj_t *week_row = lv_obj_create(calendar_container);
        lv_obj_set_size(week_row, 440, 55);
        lv_obj_set_style_bg_color(week_row, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(week_row, 0, 0);
        lv_obj_set_style_border_width(week_row, 0, 0);
        lv_obj_align(week_row, LV_ALIGN_TOP_LEFT, 0, 45 + week * 55);
        
        for (int day = 0; day < 7; day++) {
            if ((week == 0 && day < first_day) || day_count >= days) {
                continue;
            }
            
            day_count++;
            
            lv_obj_t *btn = lv_btn_create(week_row);
            lv_obj_set_size(btn, 52, 48);
            
            bool is_today = (current_year == today_year && 
                            current_month == today_month && 
                            day_count == today_day);
            
            if (is_today) {
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x4CAF50), 0);
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x388E3C), LV_STATE_PRESSED);
                lv_obj_set_style_shadow_width(btn, 8, 0);
                lv_obj_set_style_shadow_color(btn, lv_color_hex(0x4CAF50), 0);
                lv_obj_set_style_shadow_opa(btn, 80, 0);
            } else {
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x252540), 0);
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x353550), LV_STATE_PRESSED);
                lv_obj_set_style_shadow_width(btn, 4, 0);
                lv_obj_set_style_shadow_color(btn, lv_color_hex(0x000000), 0);
                lv_obj_set_style_shadow_opa(btn, 40, 0);
            }
            
            lv_obj_set_style_border_width(btn, 0, 0);
            lv_obj_set_style_radius(btn, 8, 0);
            lv_obj_align(btn, LV_ALIGN_LEFT_MID, day * 62 + 5, 0);
            
            lv_obj_t *label = lv_label_create(btn);
            char day_str[8];
            snprintf(day_str, sizeof(day_str), "%d", (uint8_t)day_count);
            lv_label_set_text(label, day_str);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(label, is_today ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xFFFFFF), 0);
            lv_obj_center(label);
        }
    }
}

static void update_month_label(void)
{
    if (!month_label) return;
    char text[64];
    snprintf(text, sizeof(text), "%s %d", month_names[current_month], current_year);
    lv_label_set_text(month_label, text);
}

static void update_today_label(void)
{
    if (!today_label) return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    char text[128];
    const char *weekday = weekday_names[t->tm_wday];
    snprintf(text, sizeof(text), "Today: %s, %d %s %d", 
             weekday, t->tm_mday, month_names[t->tm_mon], t->tm_year + 1900);
    lv_label_set_text(today_label, text);
}

static void prev_month(lv_event_t *e)
{
    (void)e;
    current_month--;
    if (current_month < 0) {
        current_month = 11;
        current_year--;
    }
    update_month_label();
    render_calendar();
}

static void next_month(lv_event_t *e)
{
    (void)e;
    current_month++;
    if (current_month > 11) {
        current_month = 0;
        current_year++;
    }
    update_month_label();
    render_calendar();
}

static void back_button_cb(lv_event_t *e)
{
    (void)e;
    app_manager_go_back();
}

lv_obj_t *calendar_app_create(void)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    today_year = t->tm_year + 1900;
    today_month = t->tm_mon;
    today_day = t->tm_mday;
    current_year = today_year;
    current_month = today_month;

    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Calendar");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    today_label = lv_label_create(scr);
    lv_obj_set_style_text_font(today_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(today_label, lv_color_hex(0x4CAF50), 0);
    lv_obj_align(today_label, LV_ALIGN_TOP_MID, 0, 75);
    update_today_label();

    lv_obj_t *nav_container = lv_obj_create(scr);
    lv_obj_set_size(nav_container, 440, 50);
    lv_obj_set_style_bg_color(nav_container, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(nav_container, 0, 0);
    lv_obj_set_style_radius(nav_container, 12, 0);
    lv_obj_set_style_shadow_width(nav_container, 6, 0);
    lv_obj_set_style_shadow_color(nav_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(nav_container, 50, 0);
    lv_obj_align(nav_container, LV_ALIGN_TOP_MID, 0, 110);

    lv_obj_t *prev_btn = lv_btn_create(nav_container);
    lv_obj_set_size(prev_btn, 45, 40);
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(prev_btn, 0, 0);
    lv_obj_set_style_radius(prev_btn, 8, 0);
    lv_obj_align(prev_btn, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_add_event_cb(prev_btn, prev_month, LV_EVENT_CLICKED, NULL);

    lv_obj_t *prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, "◀");
    lv_obj_set_style_text_font(prev_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(prev_label);

    month_label = lv_label_create(nav_container);
    lv_obj_set_style_text_font(month_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(month_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(month_label, LV_ALIGN_CENTER, 0, 0);
    update_month_label();

    lv_obj_t *next_btn = lv_btn_create(nav_container);
    lv_obj_set_size(next_btn, 45, 40);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(next_btn, 0, 0);
    lv_obj_set_style_radius(next_btn, 8, 0);
    lv_obj_align(next_btn, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_event_cb(next_btn, next_month, LV_EVENT_CLICKED, NULL);

    lv_obj_t *next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, "▶");
    lv_obj_set_style_text_font(next_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(next_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(next_label);

    calendar_container = lv_obj_create(scr);
    lv_obj_set_size(calendar_container, 440, 370);
    lv_obj_set_style_bg_color(calendar_container, lv_color_hex(0x202038), 0);
    lv_obj_set_style_border_width(calendar_container, 0, 0);
    lv_obj_set_style_radius(calendar_container, 16, 0);
    lv_obj_set_style_shadow_width(calendar_container, 8, 0);
    lv_obj_set_style_shadow_color(calendar_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(calendar_container, 60, 0);
    lv_obj_align(calendar_container, LV_ALIGN_TOP_MID, 0, 175);

    render_calendar();

    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 90, 45);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_set_style_shadow_width(back_btn, 4, 0);
    lv_obj_set_style_shadow_color(back_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(back_btn, 40, 0);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_event_cb(back_btn, back_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "← Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_label);

    ui_animation_slide(scr, LV_DIR_RIGHT, 300);

    ESP_LOGI(TAG, "Calendar app created with modern design");
    return scr;
}