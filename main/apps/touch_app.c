#include "touch_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "esp_log.h"

static const char *TAG = "TOUCH_APP";

static lv_obj_t *touch_point = NULL;
static lv_obj_t *touch_info = NULL;
static lv_obj_t *touch_count_label = NULL;
static uint32_t touch_count = 0;

static void touch_event_cb(lv_event_t *e)
{
    lv_obj_t *screen = lv_event_get_target(e);
    lv_indev_t *indev = lv_indev_get_act();
    
    if (indev) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        
        if (touch_point) {
            lv_obj_set_pos(touch_point, point.x - 20, point.y - 20);
        }
        
        if (touch_info) {
            lv_label_set_text_fmt(touch_info, "Touch: (%ld, %ld)", (long)point.x, (long)point.y);
        }
        
        touch_count++;
        if (touch_count_label) {
            lv_label_set_text_fmt(touch_count_label, "Count: %lu", touch_count);
        }
        
        ESP_LOGI(TAG, "Touch event: (%d, %d)", point.x, point.y);
    }
}

static void clear_touch(lv_event_t *e)
{
    if (touch_point) {
        lv_obj_set_pos(touch_point, -100, -100);
    }
    touch_count = 0;
    if (touch_count_label) {
        lv_label_set_text(touch_count_label, "Count: 0");
    }
}

static void back_button_cb(lv_event_t *e)
{
    app_manager_go_home();
}

lv_obj_t *touch_app_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_add_event_cb(scr, touch_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(scr, clear_touch, LV_EVENT_RELEASED, NULL);

    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Touch Test");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t *instruction = lv_label_create(scr);
    lv_label_set_text(instruction, "Tap anywhere on the screen");
    lv_obj_set_style_text_font(instruction, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(instruction, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(instruction, LV_ALIGN_TOP_MID, 0, 80);

    touch_info = lv_label_create(scr);
    lv_label_set_text(touch_info, "Touch: (0, 0)");
    lv_obj_set_style_text_font(touch_info, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(touch_info, lv_color_hex(0x00FF00), 0);
    lv_obj_align(touch_info, LV_ALIGN_BOTTOM_MID, 0, -80);

    touch_count_label = lv_label_create(scr);
    lv_label_set_text(touch_count_label, "Count: 0");
    lv_obj_set_style_text_font(touch_count_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(touch_count_label, lv_color_hex(0xFFD700), 0);
    lv_obj_align(touch_count_label, LV_ALIGN_BOTTOM_MID, 0, -50);

    touch_point = lv_obj_create(scr);
    lv_obj_set_size(touch_point, 40, 40);
    lv_obj_set_style_bg_color(touch_point, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_color(touch_point, lv_color_hex(0xFF6666), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(touch_point, 2, 0);
    lv_obj_set_style_border_color(touch_point, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(touch_point, 20, 0);
    lv_obj_set_pos(touch_point, -100, -100);

    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_event_cb(back_btn, back_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "Touch app created");
    return scr;
}
