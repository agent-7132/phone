#include "home_screen.h"
#include "status_bar.h"
#include "app_manager.h"
#include "esp_log.h"
#include "ui_utils.h"

static const char *TAG = "HOME_SCREEN";

static void app_button_click_cb(lv_event_t *e);

static lv_obj_t *create_app_button(lv_obj_t *parent, const char *name, const char *icon, int x, int y)
{
    lv_obj_t *btn_container = lv_obj_create(parent);
    lv_obj_set_size(btn_container, 88, 100);
    lv_obj_set_pos(btn_container, x, y);
    lv_obj_set_style_bg_color(btn_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(btn_container, 0, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);

    lv_obj_t *btn = lv_btn_create(btn_container);
    lv_obj_set_size(btn, 80, 80);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2d2d44), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x3d3d54), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 18, 0);
    lv_obj_set_style_shadow_width(btn, 4, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(btn, 80, 0);
    lv_obj_set_user_data(btn, (void *)name);
    lv_obj_add_event_cb(btn, app_button_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *icon_label = lv_label_create(btn);
    lv_label_set_text(icon_label, icon);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(icon_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(icon_label);

    lv_obj_t *name_label = lv_label_create(btn_container);
    lv_label_set_text(name_label, name);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(name_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(name_label, LV_ALIGN_BOTTOM_MID, 0, -2);

    return btn_container;
}

static void app_button_click_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    const char *app_name = lv_obj_get_user_data(btn);
    
    if (app_name) {
        app_manager_open_app(app_name);
    }
}

lv_obj_t *home_screen_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0f0f1a), 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    status_bar_create(scr);

    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_set_size(header, 480, 80);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 44);

    lv_obj_t *date_label = lv_label_create(header);
    lv_label_set_text(date_label, "Wednesday, July 16");
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(date_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(date_label, LV_ALIGN_TOP_LEFT, 20, 10);

    lv_obj_t *greeting_label = lv_label_create(header);
    lv_label_set_text(greeting_label, "Good Morning");
    lv_obj_set_style_text_font(greeting_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(greeting_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(greeting_label, LV_ALIGN_BOTTOM_LEFT, 20, -10);

    lv_obj_t *apps_container = lv_obj_create(scr);
    lv_obj_set_size(apps_container, 440, 580);
    lv_obj_set_style_bg_color(apps_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(apps_container, 0, 0);
    lv_obj_set_style_border_width(apps_container, 0, 0);
    lv_obj_align(apps_container, LV_ALIGN_TOP_MID, 0, 140);

    int col = 0, row = 0;
    int start_x = 10;
    int start_y = 10;
    int gap_x = 110;
    int gap_y = 115;

    int app_count = app_manager_get_app_count();
    for (int i = 0; i < app_count; i++) {
        const app_info_t *app = app_manager_get_app(i);
        if (!app) continue;

        create_app_button(apps_container, app->title, app->icon, start_x + col * gap_x, start_y + row * gap_y);

        col++;
        if (col >= 4) {
            col = 0;
            row++;
        }
    }

    lv_obj_t *dock = lv_obj_create(scr);
    lv_obj_set_size(dock, 480, 85);
    lv_obj_set_style_bg_color(dock, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(dock, 0, 0);
    lv_obj_set_style_radius(dock, 20, 0);
    lv_obj_set_style_shadow_width(dock, 8, 0);
    lv_obj_set_style_shadow_color(dock, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(dock, 100, 0);
    lv_obj_align(dock, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_t *dock_separator = lv_obj_create(dock);
    lv_obj_set_size(dock_separator, 460, 1);
    lv_obj_set_style_bg_color(dock_separator, lv_color_hex(0x3a3a5a), 0);
    lv_obj_align(dock_separator, LV_ALIGN_TOP_MID, 0, 8);

    const char *dock_apps[] = {"phone", "message", "camera", "browser"};
    const char *dock_icons[] = {"📞", "💬", "📷", "🌐"};
    
    for (int i = 0; i < 4; i++) {
        lv_obj_t *dock_btn = lv_btn_create(dock);
        lv_obj_set_size(dock_btn, 70, 70);
        lv_obj_set_style_bg_color(dock_btn, lv_color_hex(0x2d2d44), 0);
        lv_obj_set_style_bg_color(dock_btn, lv_color_hex(0x3d3d54), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(dock_btn, 0, 0);
        lv_obj_set_style_radius(dock_btn, 16, 0);
        lv_obj_align(dock_btn, LV_ALIGN_BOTTOM_MID, (i - 1.5) * 95, -5);
        
        lv_obj_t *dock_icon = lv_label_create(dock_btn);
        lv_label_set_text(dock_icon, dock_icons[i]);
        lv_obj_set_style_text_font(dock_icon, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(dock_icon, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(dock_icon);
    }

    ESP_LOGI(TAG, "Home screen created with %d apps", app_count);
    return scr;
}