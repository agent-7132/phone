#include "home_screen.h"
#include "status_bar.h"
#include "app_manager.h"
#include "esp_log.h"

static const char *TAG = "HOME_SCREEN";

static void app_button_click_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    const char *app_name = lv_obj_get_user_data(btn);
    
    if (app_name) {
        app_manager_open_app(app_name);
    }
}

static lv_obj_t *create_app_button(lv_obj_t *parent, const char *name, const char *icon, int x, int y)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 80, 80);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2d2d44), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x3d3d54), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_user_data(btn, (void *)name);
    lv_obj_add_event_cb(btn, app_button_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, icon);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(label);

    return btn;
}

static void add_app_label(lv_obj_t *parent, const char *name, int x, int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, name);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_pos(label, x + 10, y + 85);
}

lv_obj_t *home_screen_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "ESP32-P4 Phone");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    int col = 0, row = 0;
    int start_x = 40;
    int start_y = 100;
    int gap_x = 100;
    int gap_y = 110;

    int app_count = app_manager_get_app_count();
    for (int i = 0; i < app_count; i++) {
        const app_info_t *app = app_manager_get_app(i);
        if (!app) continue;

        create_app_button(scr, app->name, app->icon, start_x + col * gap_x, start_y + row * gap_y);
        add_app_label(scr, app->title, start_x + col * gap_x, start_y + row * gap_y);

        col++;
        if (col >= 4) {
            col = 0;
            row++;
        }
    }

    ESP_LOGI(TAG, "Home screen created with %d apps", app_count);
    return scr;
}
