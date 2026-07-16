#include "settings_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "bsp/display.h"
#include "theme_manager.h"
#include "sensor_manager.h"
#include "i18n_manager.h"
#include "permission_manager.h"
#include "ui_animation.h"
#include "ui_feedback.h"
#include "ui_utils.h"
#include "settings_manager.h"
#include "esp_log.h"

static const char *TAG = "SETTINGS_APP";

static lv_obj_t *brightness_slider = NULL;
static lv_obj_t *brightness_label = NULL;
static lv_obj_t *theme_btn = NULL;
static lv_obj_t *sensor_label = NULL;
static lv_obj_t *lang_dropdown = NULL;
static lv_obj_t *lang_label = NULL;
static lv_obj_t *search_input = NULL;
static lv_obj_t *search_results = NULL;
static lv_obj_t *settings_container = NULL;

typedef enum {
    SETTING_ITEM_BRIGHTNESS,
    SETTING_ITEM_BACKLIGHT_ON,
    SETTING_ITEM_BACKLIGHT_OFF,
    SETTING_ITEM_THEME,
    SETTING_ITEM_SENSOR,
    SETTING_ITEM_LANGUAGE,
    SETTING_ITEM_INFO
} setting_item_t;

static void brightness_changed(lv_event_t *e)
{
    int brightness = lv_slider_get_value(brightness_slider);
    esp_err_t ret = settings_manager_set_brightness(brightness);
    
    if (ret == ESP_OK) {
        char bright_str[32];
        snprintf(bright_str, sizeof(bright_str), "%d%%", brightness);
        lv_label_set_text(brightness_label, bright_str);
        ESP_LOGI(TAG, "Brightness set to %d%%", brightness);
    } else {
        lv_label_set_text(brightness_label, "Failed");
        ESP_LOGE(TAG, "Brightness set failed: %s", esp_err_to_name(ret));
    }
}

static void backlight_on(lv_event_t *e)
{
    (void)e;
    esp_err_t ret = bsp_display_backlight_on();
    if (ret == ESP_OK) {
        ui_feedback_show_toast("Backlight", "Turned ON", UI_FEEDBACK_TYPE_SUCCESS, 1500);
        ESP_LOGI(TAG, "Backlight turned on");
    } else {
        ui_feedback_show_toast("Error", "Backlight ON failed", UI_FEEDBACK_TYPE_ERROR, 1500);
        ESP_LOGE(TAG, "Backlight ON failed: %s", esp_err_to_name(ret));
    }
}

static void backlight_off(lv_event_t *e)
{
    (void)e;
    esp_err_t ret = bsp_display_backlight_off();
    if (ret == ESP_OK) {
        ui_feedback_show_toast("Backlight", "Turned OFF", UI_FEEDBACK_TYPE_INFO, 1500);
        ESP_LOGI(TAG, "Backlight turned off");
    } else {
        ui_feedback_show_toast("Error", "Backlight OFF failed", UI_FEEDBACK_TYPE_ERROR, 1500);
        ESP_LOGE(TAG, "Backlight OFF failed: %s", esp_err_to_name(ret));
    }
}

static void toggle_theme(lv_event_t *e)
{
    (void)e;
    theme_mode_t current_mode = theme_manager_get_mode();
    theme_mode_t new_mode = (current_mode == THEME_MODE_DARK) ? THEME_MODE_LIGHT : THEME_MODE_DARK;
    theme_manager_set_mode(new_mode);
    
    lv_obj_t *label = lv_obj_get_child(theme_btn, 0);
    if (label && lv_obj_check_type(label, &lv_label_class)) {
        lv_label_set_text(label, new_mode == THEME_MODE_DARK ? "Dark" : "Light");
    }
    
    ui_feedback_show_toast("Theme Changed", 
                          new_mode == THEME_MODE_DARK ? "Dark mode enabled" : "Light mode enabled", 
                          UI_FEEDBACK_TYPE_INFO, 2000);
    
    ESP_LOGI(TAG, "Theme changed to %s", new_mode == THEME_MODE_DARK ? "dark" : "light");
}

static void read_sensor_data(lv_event_t *e)
{
    (void)e;
    sensor_data_t data;
    esp_err_t ret = sensor_manager_read("sht3x", &data);
    
    if (ret == ESP_OK) {
        char sensor_str[64];
        snprintf(sensor_str, sizeof(sensor_str), "%.2f°C  %.2f%%", 
                 data.env.temperature, data.env.humidity);
        lv_label_set_text(sensor_label, sensor_str);
        ESP_LOGI(TAG, "Sensor data read: %.2f°C, %.2f%%", data.env.temperature, data.env.humidity);
        
        status_bar_set_sensor_data(data.env.temperature, data.env.humidity);
    } else {
        lv_label_set_text(sensor_label, "No sensor");
        ESP_LOGE(TAG, "Sensor read failed: %s", esp_err_to_name(ret));
    }
}

static void lang_selected_cb(lv_event_t *e)
{
    lv_obj_t *dropdown = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    
    i18n_manager_set_language((language_t)selected);
    
    const char *lang_names[] = {"English", "中文", "日本語", "한국어", "Français"};
    lv_label_set_text(lang_label, lang_names[selected]);
    
    ESP_LOGI(TAG, "Language changed to: %d", selected);
}

static void show_settings_item(setting_item_t item)
{
    lv_obj_add_flag(search_results, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(settings_container, LV_OBJ_FLAG_HIDDEN);
    
    switch (item) {
        case SETTING_ITEM_BRIGHTNESS:
            lv_obj_scroll_to_y(settings_container, -100, LV_ANIM_ON);
            break;
        case SETTING_ITEM_THEME:
            lv_obj_scroll_to_y(settings_container, -350, LV_ANIM_ON);
            break;
        case SETTING_ITEM_SENSOR:
            lv_obj_scroll_to_y(settings_container, -500, LV_ANIM_ON);
            break;
        case SETTING_ITEM_LANGUAGE:
            lv_obj_scroll_to_y(settings_container, -600, LV_ANIM_ON);
            break;
        default:
            lv_obj_scroll_to_y(settings_container, 0, LV_ANIM_ON);
            break;
    }
}

static void search_result_click_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    setting_item_t item = (setting_item_t)(intptr_t)lv_obj_get_user_data(btn);
    show_settings_item(item);
}

static void search_input_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    const char *query = lv_textarea_get_text(ta);
    
    lv_obj_clean(search_results);
    
    if (!query || query[0] == '\0') {
        lv_obj_add_flag(search_results, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(settings_container, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    
    lv_obj_clear_flag(search_results, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settings_container, LV_OBJ_FLAG_HIDDEN);
    
    const char *setting_items[] = {"Brightness", "Backlight", "Theme", "Sensor", "Language", "Info"};
    const char *setting_icons[] = {"☀️", "💡", "🎨", "🔬", "🌐", "ℹ️"};
    setting_item_t setting_ids[] = {SETTING_ITEM_BRIGHTNESS, SETTING_ITEM_BACKLIGHT_ON, SETTING_ITEM_THEME, 
                                    SETTING_ITEM_SENSOR, SETTING_ITEM_LANGUAGE, SETTING_ITEM_INFO};
    
    for (int i = 0; i < 6; i++) {
        if (strstr(setting_items[i], query) || 
            strcasestr(setting_items[i], query)) {
            lv_obj_t *btn = lv_btn_create(search_results);
            lv_obj_set_size(btn, 360, 60);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x252540), 0);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x3a3a5a), LV_STATE_PRESSED);
            lv_obj_set_style_border_width(btn, 0, 0);
            lv_obj_set_style_radius(btn, 8, 0);
            
            lv_obj_t *icon_label = lv_label_create(btn);
            lv_label_set_text(icon_label, setting_icons[i]);
            lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_14, 0);
            lv_obj_align(icon_label, LV_ALIGN_LEFT_MID, 15, 0);
            
            lv_obj_t *name_label = lv_label_create(btn);
            lv_label_set_text(name_label, setting_items[i]);
            lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(name_label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 55, 0);
            
            lv_obj_t *arrow_label = lv_label_create(btn);
            lv_label_set_text(arrow_label, "›");
            lv_obj_set_style_text_font(arrow_label, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(arrow_label, lv_color_hex(0x666666), 0);
            lv_obj_align(arrow_label, LV_ALIGN_RIGHT_MID, -15, 0);
            
            lv_obj_set_user_data(btn, (void *)(intptr_t)setting_ids[i]);
            lv_obj_add_event_cb(btn, search_result_click_cb, LV_EVENT_CLICKED, NULL);
        }
    }
}

static lv_obj_t *create_settings_card(lv_obj_t *parent, const char *title, int y_pos)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 440, 180);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_shadow_width(card, 4, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(card, 60, 0);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, y_pos);
    
    lv_obj_t *title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 20, 15);
    
    return card;
}

static lv_obj_t *create_settings_row(lv_obj_t *parent, const char *icon, const char *label, 
                                     lv_event_cb_t cb, void *user_data, int y_offset)
{
    lv_obj_t *row = lv_btn_create(parent);
    lv_obj_set_size(row, 400, 50);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x2a2a4e), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 20, y_offset);
    
    lv_obj_t *icon_label = lv_label_create(row);
    lv_label_set_text(icon_label, icon);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_14, 0);
    lv_obj_align(icon_label, LV_ALIGN_LEFT_MID, 10, 0);
    
    lv_obj_t *text_label = lv_label_create(row);
    lv_label_set_text(text_label, label);
    lv_obj_set_style_text_font(text_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(text_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(text_label, LV_ALIGN_LEFT_MID, 55, 0);
    
    lv_obj_t *arrow_label = lv_label_create(row);
    lv_label_set_text(arrow_label, "›");
    lv_obj_set_style_text_font(arrow_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(arrow_label, lv_color_hex(0x666666), 0);
    lv_obj_align(arrow_label, LV_ALIGN_RIGHT_MID, -15, 0);
    
    if (cb) {
        lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, user_data);
    }
    
    return row;
}

lv_obj_t *settings_app_create(void)
{
    lv_obj_t *scr = ui_utils_create_screen();

    status_bar_create(scr);

    ui_utils_create_title(scr, "Settings");

    search_input = lv_textarea_create(scr);
    lv_obj_set_size(search_input, 400, 45);
    lv_textarea_set_placeholder_text(search_input, "Search settings...");
    lv_obj_set_style_bg_color(search_input, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_color(search_input, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_border_width(search_input, 0, 0);
    lv_obj_set_style_text_color(search_input, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(search_input, 12, 0);
    lv_obj_align(search_input, LV_ALIGN_TOP_MID, 0, 90);
    lv_obj_add_event_cb(search_input, search_input_cb, LV_EVENT_VALUE_CHANGED, NULL);

    search_results = lv_obj_create(scr);
    lv_obj_set_size(search_results, 440, 500);
    lv_obj_set_style_bg_color(search_results, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(search_results, 0, 0);
    lv_obj_align(search_results, LV_ALIGN_TOP_MID, 0, 150);
    lv_obj_add_flag(search_results, LV_OBJ_FLAG_HIDDEN);

    settings_container = lv_obj_create(scr);
    lv_obj_set_size(settings_container, 480, 650);
    lv_obj_set_style_bg_color(settings_container, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(settings_container, 0, 0);
    lv_obj_set_scrollbar_mode(settings_container, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_align(settings_container, LV_ALIGN_TOP_MID, 0, 150);

    lv_obj_t *display_card = create_settings_card(settings_container, "Display", 20);
    
    lv_obj_t *brightness_row = lv_obj_create(display_card);
    lv_obj_set_size(brightness_row, 400, 80);
    lv_obj_set_style_bg_color(brightness_row, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(brightness_row, 0, 0);
    lv_obj_set_style_radius(brightness_row, 8, 0);
    lv_obj_align(brightness_row, LV_ALIGN_TOP_LEFT, 20, 45);
    
    lv_obj_t *bright_icon = lv_label_create(brightness_row);
    lv_label_set_text(bright_icon, "☀️");
    lv_obj_set_style_text_font(bright_icon, &lv_font_montserrat_14, 0);
    lv_obj_align(bright_icon, LV_ALIGN_LEFT_MID, 10, 0);
    
    lv_obj_t *bright_text = lv_label_create(brightness_row);
    lv_label_set_text(bright_text, "Brightness");
    lv_obj_set_style_text_font(bright_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(bright_text, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(bright_text, LV_ALIGN_TOP_LEFT, 55, 10);
    
    int current_brightness = settings_manager_get_brightness();
    
    brightness_slider = lv_slider_create(brightness_row);
    lv_obj_set_size(brightness_slider, 220, 6);
    lv_slider_set_range(brightness_slider, 0, 100);
    lv_slider_set_value(brightness_slider, current_brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x4CAF50), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x4CAF50), LV_PART_KNOB);
    lv_obj_set_style_border_width(brightness_slider, 0, 0);
    lv_obj_set_style_radius(brightness_slider, 3, 0);
    lv_obj_align(brightness_slider, LV_ALIGN_BOTTOM_LEFT, 55, -12);
    lv_obj_add_event_cb(brightness_slider, brightness_changed, LV_EVENT_VALUE_CHANGED, NULL);
    
    brightness_label = lv_label_create(brightness_row);
    char bright_str[8];
    snprintf(bright_str, sizeof(bright_str), "%d%%", current_brightness);
    lv_label_set_text(brightness_label, bright_str);
    lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(brightness_label, lv_color_hex(0x4CAF50), 0);
    lv_obj_align(brightness_label, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    
    lv_obj_t *backlight_row = lv_obj_create(display_card);
    lv_obj_set_size(backlight_row, 400, 40);
    lv_obj_set_style_bg_color(backlight_row, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(backlight_row, 0, 0);
    lv_obj_set_style_radius(backlight_row, 8, 0);
    lv_obj_align(backlight_row, LV_ALIGN_TOP_LEFT, 20, 135);
    
    lv_obj_t *backlight_icon = lv_label_create(backlight_row);
    lv_label_set_text(backlight_icon, "💡");
    lv_obj_set_style_text_font(backlight_icon, &lv_font_montserrat_14, 0);
    lv_obj_align(backlight_icon, LV_ALIGN_LEFT_MID, 10, 0);
    
    lv_obj_t *backlight_text = lv_label_create(backlight_row);
    lv_label_set_text(backlight_text, "Backlight");
    lv_obj_set_style_text_font(backlight_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(backlight_text, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(backlight_text, LV_ALIGN_LEFT_MID, 55, 0);
    
    lv_obj_t *backlight_on_btn = lv_btn_create(backlight_row);
    lv_obj_set_size(backlight_on_btn, 60, 28);
    lv_obj_set_style_bg_color(backlight_on_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_bg_color(backlight_on_btn, lv_color_hex(0x388E3C), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(backlight_on_btn, 0, 0);
    lv_obj_set_style_radius(backlight_on_btn, 14, 0);
    lv_obj_align(backlight_on_btn, LV_ALIGN_RIGHT_MID, -70, 0);
    lv_obj_add_event_cb(backlight_on_btn, backlight_on, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *backlight_on_label = lv_label_create(backlight_on_btn);
    lv_label_set_text(backlight_on_label, "ON");
    lv_obj_set_style_text_font(backlight_on_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(backlight_on_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(backlight_on_label);
    
    lv_obj_t *backlight_off_btn = lv_btn_create(backlight_row);
    lv_obj_set_size(backlight_off_btn, 60, 28);
    lv_obj_set_style_bg_color(backlight_off_btn, lv_color_hex(0xE57373), 0);
    lv_obj_set_style_bg_color(backlight_off_btn, lv_color_hex(0xE53935), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(backlight_off_btn, 0, 0);
    lv_obj_set_style_radius(backlight_off_btn, 14, 0);
    lv_obj_align(backlight_off_btn, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_event_cb(backlight_off_btn, backlight_off, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *backlight_off_label = lv_label_create(backlight_off_btn);
    lv_label_set_text(backlight_off_label, "OFF");
    lv_obj_set_style_text_font(backlight_off_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(backlight_off_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(backlight_off_label);

    lv_obj_t *appearance_card = create_settings_card(settings_container, "Appearance", 220);
    
    theme_btn = create_settings_row(appearance_card, "🎨", 
                                   theme_manager_get_mode() == THEME_MODE_DARK ? "Dark" : "Light", 
                                   toggle_theme, NULL, 45);

    lv_obj_t *system_card = create_settings_card(settings_container, "System", 420);
    
    lv_obj_t *sensor_row = lv_obj_create(system_card);
    lv_obj_set_size(sensor_row, 400, 80);
    lv_obj_set_style_bg_color(sensor_row, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(sensor_row, 0, 0);
    lv_obj_set_style_radius(sensor_row, 8, 0);
    lv_obj_align(sensor_row, LV_ALIGN_TOP_LEFT, 20, 45);
    
    lv_obj_t *sensor_icon = lv_label_create(sensor_row);
    lv_label_set_text(sensor_icon, "🔬");
    lv_obj_set_style_text_font(sensor_icon, &lv_font_montserrat_14, 0);
    lv_obj_align(sensor_icon, LV_ALIGN_LEFT_MID, 10, 0);
    
    lv_obj_t *sensor_text = lv_label_create(sensor_row);
    lv_label_set_text(sensor_text, "Sensors");
    lv_obj_set_style_text_font(sensor_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sensor_text, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(sensor_text, LV_ALIGN_TOP_LEFT, 55, 10);
    
    sensor_label = lv_label_create(sensor_row);
    lv_label_set_text(sensor_label, "Tap to read");
    lv_obj_set_style_text_font(sensor_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sensor_label, lv_color_hex(0x888888), 0);
    lv_obj_align(sensor_label, LV_ALIGN_BOTTOM_LEFT, 55, -10);
    
    lv_obj_t *sensor_btn = lv_btn_create(sensor_row);
    lv_obj_set_size(sensor_btn, 80, 28);
    lv_obj_set_style_bg_color(sensor_btn, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_bg_color(sensor_btn, lv_color_hex(0x1976D2), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(sensor_btn, 0, 0);
    lv_obj_set_style_radius(sensor_btn, 14, 0);
    lv_obj_align(sensor_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_add_event_cb(sensor_btn, read_sensor_data, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *sensor_btn_label = lv_label_create(sensor_btn);
    lv_label_set_text(sensor_btn_label, "Read");
    lv_obj_set_style_text_font(sensor_btn_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sensor_btn_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(sensor_btn_label);

    lv_obj_t *language_row = create_settings_row(system_card, "🌐", "Language", NULL, NULL, 135);
    
    lang_dropdown = lv_dropdown_create(language_row);
    lv_dropdown_set_options(lang_dropdown, "English\n中文\n日本語\n한국어\nFrançais");
    lv_dropdown_set_selected(lang_dropdown, i18n_manager_get_current_language());
    lv_obj_set_size(lang_dropdown, 100, 30);
    lv_obj_set_style_bg_color(lang_dropdown, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_text_color(lang_dropdown, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(lang_dropdown, 0, 0);
    lv_obj_set_style_radius(lang_dropdown, 6, 0);
    lv_obj_align(lang_dropdown, LV_ALIGN_RIGHT_MID, -15, 0);
    lv_obj_add_event_cb(lang_dropdown, lang_selected_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    lang_label = lv_label_create(language_row);
    const char *lang_names[] = {"English", "中文", "日本語", "한국어", "Français"};
    lv_label_set_text(lang_label, lang_names[i18n_manager_get_current_language()]);
    lv_obj_set_style_text_font(lang_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lang_label, lv_color_hex(0x888888), 0);
    lv_obj_align(lang_label, LV_ALIGN_RIGHT_MID, -120, 0);

    lv_obj_t *info_card = lv_obj_create(settings_container);
    lv_obj_set_size(info_card, 440, 100);
    lv_obj_set_style_bg_color(info_card, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(info_card, 0, 0);
    lv_obj_set_style_radius(info_card, 16, 0);
    lv_obj_align(info_card, LV_ALIGN_TOP_MID, 0, 620);
    
    lv_obj_t *info_title = lv_label_create(info_card);
    lv_label_set_text(info_title, "About");
    lv_obj_set_style_text_font(info_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(info_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(info_title, LV_ALIGN_TOP_LEFT, 20, 15);
    
    lv_obj_t *info_text = lv_label_create(info_card);
    lv_label_set_text(info_text, "ESP32-P4 PhoneUI v1.0");
    lv_obj_set_style_text_font(info_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(info_text, lv_color_hex(0x888888), 0);
    lv_obj_align(info_text, LV_ALIGN_TOP_LEFT, 20, 45);
    
    lv_obj_t *info_specs = lv_label_create(info_card);
    lv_label_set_text(info_specs, "ST7102 LCD | ST7123 Touch | 32MB PSRAM");
    lv_obj_set_style_text_font(info_specs, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(info_specs, lv_color_hex(0x666666), 0);
    lv_obj_align(info_specs, LV_ALIGN_TOP_LEFT, 20, 70);

    ui_utils_create_back_button(scr);

    ui_animation_fade(scr, true, 300);
    
    ESP_LOGI(TAG, "Settings app created with card design");
    return scr;
}