#include "settings_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "bsp/display.h"
#include "esp_log.h"

static const char *TAG = "SETTINGS_APP";

static lv_obj_t *brightness_slider = NULL;
static lv_obj_t *brightness_label = NULL;

static void brightness_changed(lv_event_t *e)
{
    int brightness = lv_slider_get_value(brightness_slider);
    esp_err_t ret = bsp_display_brightness_set(brightness);
    
    if (ret == ESP_OK) {
        char bright_str[32];
        snprintf(bright_str, sizeof(bright_str), "Brightness: %d%%", brightness);
        lv_label_set_text(brightness_label, bright_str);
        ESP_LOGI(TAG, "Brightness set to %d%%", brightness);
    } else {
        lv_label_set_text(brightness_label, "Brightness change failed");
        ESP_LOGE(TAG, "Brightness set failed: %s", esp_err_to_name(ret));
    }
}

static void backlight_on(lv_event_t *e)
{
    esp_err_t ret = bsp_display_backlight_on();
    if (ret == ESP_OK) {
        lv_label_set_text(brightness_label, "Backlight ON");
        ESP_LOGI(TAG, "Backlight turned on");
    } else {
        lv_label_set_text(brightness_label, "Backlight ON failed");
        ESP_LOGE(TAG, "Backlight ON failed: %s", esp_err_to_name(ret));
    }
}

static void backlight_off(lv_event_t *e)
{
    esp_err_t ret = bsp_display_backlight_off();
    if (ret == ESP_OK) {
        lv_label_set_text(brightness_label, "Backlight OFF");
        ESP_LOGI(TAG, "Backlight turned off");
    } else {
        lv_label_set_text(brightness_label, "Backlight OFF failed");
        ESP_LOGE(TAG, "Backlight OFF failed: %s", esp_err_to_name(ret));
    }
}

static void back_button_cb(lv_event_t *e)
{
    app_manager_go_home();
}

lv_obj_t *settings_app_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t *bright_title = lv_label_create(scr);
    lv_label_set_text(bright_title, "Display Brightness");
    lv_obj_set_style_text_font(bright_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(bright_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(bright_title, LV_ALIGN_TOP_MID, 0, 100);

    brightness_slider = lv_slider_create(scr);
    lv_obj_set_size(brightness_slider, 300, 25);
    lv_slider_set_range(brightness_slider, 0, 100);
    lv_slider_set_value(brightness_slider, 80, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x5a5a8a), LV_PART_INDICATOR);
    lv_obj_set_style_border_width(brightness_slider, 0, 0);
    lv_obj_align(brightness_slider, LV_ALIGN_TOP_MID, 0, 140);
    lv_obj_add_event_cb(brightness_slider, brightness_changed, LV_EVENT_VALUE_CHANGED, NULL);

    brightness_label = lv_label_create(scr);
    lv_label_set_text(brightness_label, "Brightness: 80%");
    lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(brightness_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(brightness_label, LV_ALIGN_TOP_MID, 0, 180);

    lv_obj_t *backlight_on_btn = lv_btn_create(scr);
    lv_obj_set_size(backlight_on_btn, 150, 50);
    lv_obj_set_style_bg_color(backlight_on_btn, lv_color_hex(0x4a8a6a), 0);
    lv_obj_set_style_bg_color(backlight_on_btn, lv_color_hex(0x5a9a7a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(backlight_on_btn, 0, 0);
    lv_obj_set_style_radius(backlight_on_btn, 8, 0);
    lv_obj_align(backlight_on_btn, LV_ALIGN_CENTER, -80, 80);
    lv_obj_add_event_cb(backlight_on_btn, backlight_on, LV_EVENT_CLICKED, NULL);

    lv_obj_t *on_label = lv_label_create(backlight_on_btn);
    lv_label_set_text(on_label, "Backlight ON");
    lv_obj_set_style_text_font(on_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(on_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(on_label);

    lv_obj_t *backlight_off_btn = lv_btn_create(scr);
    lv_obj_set_size(backlight_off_btn, 150, 50);
    lv_obj_set_style_bg_color(backlight_off_btn, lv_color_hex(0x8a4a4a), 0);
    lv_obj_set_style_bg_color(backlight_off_btn, lv_color_hex(0x9a5a5a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(backlight_off_btn, 0, 0);
    lv_obj_set_style_radius(backlight_off_btn, 8, 0);
    lv_obj_align(backlight_off_btn, LV_ALIGN_CENTER, 80, 80);
    lv_obj_add_event_cb(backlight_off_btn, backlight_off, LV_EVENT_CLICKED, NULL);

    lv_obj_t *off_label = lv_label_create(backlight_off_btn);
    lv_label_set_text(off_label, "Backlight OFF");
    lv_obj_set_style_text_font(off_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(off_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(off_label);

    lv_obj_t *info_label = lv_label_create(scr);
    lv_label_set_text(info_label, "ESP32-P4 PhoneUI\n\n- ST7102 LCD (480x800)\n- ST7123 Touch\n- 32MB PSRAM\n- 16MB Flash");
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(info_label, lv_color_hex(0x888888), 0);
    lv_obj_align(info_label, LV_ALIGN_BOTTOM_MID, 0, -80);

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

    ESP_LOGI(TAG, "Settings app created");
    return scr;
}
