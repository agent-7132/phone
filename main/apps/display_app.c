#include "display_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "ui_animation.h"
#include "bsp/display.h"
#include "esp_log.h"

static const char *TAG = "DISPLAY_APP";

static lv_obj_t *color_rect = NULL;
static lv_obj_t *color_label = NULL;
static uint32_t color_index = 0;

static const lv_color_t test_colors[] = {
    {0xFF, 0x00, 0x00}, {0x00, 0xFF, 0x00}, {0x00, 0x00, 0xFF},
    {0xFF, 0xFF, 0x00}, {0xFF, 0x00, 0xFF}, {0x00, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF}, {0x00, 0x00, 0x00},
    {0xFF, 0xA5, 0x00}, {0x80, 0x00, 0x80}, {0x00, 0xCE, 0xD1}
};

static const char *color_names[] = {
    "Red", "Green", "Blue", "Yellow", "Magenta", "Cyan",
    "White", "Black", "Orange", "Purple", "Teal"
};

static void change_color(lv_event_t *e)
{
    color_index = (color_index + 1) % (sizeof(test_colors) / sizeof(test_colors[0]));
    
    if (color_rect) {
        lv_obj_set_style_bg_color(color_rect, test_colors[color_index], 0);
    }
    
    if (color_label) {
        lv_label_set_text_fmt(color_label, "Color: %s", color_names[color_index]);
    }
}

static void back_button_cb(lv_event_t *e)
{
    app_manager_go_home();
}

void display_app_on_exit(void)
{
    color_rect = NULL;
    color_label = NULL;
    ESP_LOGI(TAG, "Display app exited");
}

lv_obj_t *display_app_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Display Test");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t *color_card = lv_obj_create(scr);
    lv_obj_set_size(color_card, 400, 420);
    lv_obj_set_style_bg_color(color_card, lv_color_hex(0x202038), 0);
    lv_obj_set_style_border_width(color_card, 0, 0);
    lv_obj_set_style_radius(color_card, 16, 0);
    lv_obj_set_style_shadow_width(color_card, 8, 0);
    lv_obj_set_style_shadow_color(color_card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(color_card, 60, 0);
    lv_obj_align(color_card, LV_ALIGN_CENTER, 0, 10);

    color_rect = lv_obj_create(color_card);
    lv_obj_set_size(color_rect, 320, 320);
    lv_obj_set_style_bg_color(color_rect, test_colors[color_index], 0);
    lv_obj_set_style_border_width(color_rect, 0, 0);
    lv_obj_set_style_radius(color_rect, 16, 0);
    lv_obj_set_style_shadow_width(color_rect, 8, 0);
    lv_obj_set_style_shadow_color(color_rect, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(color_rect, 50, 0);
    lv_obj_align(color_rect, LV_ALIGN_TOP_MID, 0, 20);

    color_label = lv_label_create(color_card);
    lv_label_set_text_fmt(color_label, "Color: %s", color_names[color_index]);
    lv_obj_set_style_text_font(color_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(color_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(color_label, LV_ALIGN_TOP_MID, 0, 360);

    lv_obj_t *next_btn = lv_btn_create(scr);
    lv_obj_set_size(next_btn, 200, 56);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x388E3C), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(next_btn, 0, 0);
    lv_obj_set_style_radius(next_btn, 12, 0);
    lv_obj_set_style_shadow_width(next_btn, 6, 0);
    lv_obj_set_style_shadow_color(next_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(next_btn, 50, 0);
    lv_obj_align(next_btn, LV_ALIGN_BOTTOM_MID, 0, -80);
    lv_obj_add_event_cb(next_btn, change_color, LV_EVENT_CLICKED, NULL);

    lv_obj_t *next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, "Next Color");
    lv_obj_set_style_text_font(next_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(next_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(next_label);

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

    ESP_LOGI(TAG, "Display app created with modern design");
    return scr;
}
