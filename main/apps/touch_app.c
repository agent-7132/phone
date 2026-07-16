#include "touch_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "ui_animation.h"
#include "gesture_manager.h"
#include "esp_log.h"

static const char *TAG = "TOUCH_APP";

static lv_obj_t *touch_point = NULL;
static lv_obj_t *touch_info = NULL;
static lv_obj_t *touch_count_label = NULL;
static lv_obj_t *gesture_info = NULL;
static uint32_t touch_count = 0;

static void gesture_handler(gesture_type_t type, lv_point_t start, lv_point_t end)
{
    const char *gesture_names[] = {
        "NONE", "SWIPE_LEFT", "SWIPE_RIGHT", "SWIPE_UP", "SWIPE_DOWN",
        "TAP", "DOUBLE_TAP", "LONG_PRESS", "PINCH_IN", "PINCH_OUT"
    };
    
    if (gesture_info) {
        lv_label_set_text_fmt(gesture_info, "Gesture: %s (%ld,%ld)-(%ld,%ld)",
            gesture_names[type], (long)start.x, (long)start.y, (long)end.x, (long)end.y);
    }
    
    ESP_LOGI(TAG, "Gesture detected: %s", gesture_names[type]);
}

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
    (void)e;
    if (touch_point) {
        lv_obj_set_pos(touch_point, -100, -100);
    }
}

static void back_button_cb(lv_event_t *e)
{
    (void)e;
    gesture_manager_unregister_callback(gesture_handler);
    app_manager_go_home();
}

void touch_app_on_exit(void)
{
    gesture_manager_unregister_callback(gesture_handler);
    touch_point = NULL;
    touch_info = NULL;
    touch_count_label = NULL;
    gesture_info = NULL;
    touch_count = 0;
    ESP_LOGI(TAG, "Touch app exited and cleaned up");
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
    lv_label_set_text(title, "Touch & Gesture Test");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t *instruction = lv_label_create(scr);
    lv_label_set_text(instruction, "Tap, Double-tap, Long press or Swipe");
    lv_obj_set_style_text_font(instruction, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(instruction, lv_color_hex(0x87CEEB), 0);
    lv_obj_align(instruction, LV_ALIGN_TOP_MID, 0, 80);

    lv_obj_t *info_card = lv_obj_create(scr);
    lv_obj_set_size(info_card, 400, 180);
    lv_obj_set_style_bg_color(info_card, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(info_card, 0, 0);
    lv_obj_set_style_radius(info_card, 16, 0);
    lv_obj_set_style_shadow_width(info_card, 8, 0);
    lv_obj_set_style_shadow_color(info_card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(info_card, 60, 0);
    lv_obj_align(info_card, LV_ALIGN_BOTTOM_MID, 0, 90);

    touch_info = lv_label_create(info_card);
    lv_label_set_text(touch_info, "Touch: (0, 0)");
    lv_obj_set_style_text_font(touch_info, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(touch_info, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_pad_all(touch_info, 15, 0);
    lv_obj_align(touch_info, LV_ALIGN_TOP_LEFT, 0, 0);

    gesture_info = lv_label_create(info_card);
    lv_label_set_text(gesture_info, "Gesture: NONE");
    lv_obj_set_style_text_font(gesture_info, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(gesture_info, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_pad_all(gesture_info, 15, 0);
    lv_obj_align(gesture_info, LV_ALIGN_TOP_LEFT, 0, 40);

    touch_count_label = lv_label_create(info_card);
    lv_label_set_text(touch_count_label, "Count: 0");
    lv_obj_set_style_text_font(touch_count_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(touch_count_label, lv_color_hex(0x87CEEB), 0);
    lv_obj_set_style_pad_all(touch_count_label, 15, 0);
    lv_obj_align(touch_count_label, LV_ALIGN_TOP_LEFT, 0, 80);

    touch_point = lv_obj_create(scr);
    lv_obj_set_size(touch_point, 44, 44);
    lv_obj_set_style_bg_color(touch_point, lv_color_hex(0xE53935), 0);
    lv_obj_set_style_bg_color(touch_point, lv_color_hex(0xC62828), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(touch_point, 0, 0);
    lv_obj_set_style_radius(touch_point, 22, 0);
    lv_obj_set_style_shadow_width(touch_point, 8, 0);
    lv_obj_set_style_shadow_color(touch_point, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(touch_point, 60, 0);
    lv_obj_set_pos(touch_point, -100, -100);

    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 100, 45);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 10, 0);
    lv_obj_set_style_shadow_width(back_btn, 4, 0);
    lv_obj_set_style_shadow_color(back_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(back_btn, 40, 0);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_event_cb(back_btn, back_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_label);

    ui_animation_slide(scr, LV_DIR_RIGHT, 300);

    gesture_manager_register_callback(gesture_handler);

    ESP_LOGI(TAG, "Touch app created with modern design");
    return scr;
}