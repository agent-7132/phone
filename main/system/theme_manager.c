#include "theme_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "THEME_MANAGER";

static theme_mode_t s_current_mode = THEME_MODE_DARK;

static const theme_colors_t s_dark_colors = {
    .bg_primary = LV_COLOR_MAKE(0x1a, 0x1a, 0x2e),
    .bg_secondary = LV_COLOR_MAKE(0x16, 0x21, 0x3e),
    .bg_card = LV_COLOR_MAKE(0x25, 0x25, 0x40),
    .bg_button = LV_COLOR_MAKE(0x4a, 0x4a, 0x6a),
    .bg_button_pressed = LV_COLOR_MAKE(0x5a, 0x5a, 0x8a),
    .text_primary = LV_COLOR_MAKE(0xff, 0xff, 0xff),
    .text_secondary = LV_COLOR_MAKE(0xaa, 0xaa, 0xaa),
    .text_highlight = LV_COLOR_MAKE(0x00, 0xff, 0x00),
    .border = LV_COLOR_MAKE(0x3a, 0x3a, 0x5a),
    .accent = LV_COLOR_MAKE(0x4a, 0x8a, 0x6a),
};

static const theme_colors_t s_light_colors = {
    .bg_primary = LV_COLOR_MAKE(0xff, 0xff, 0xff),
    .bg_secondary = LV_COLOR_MAKE(0xf5, 0xf5, 0xf5),
    .bg_card = LV_COLOR_MAKE(0xff, 0xff, 0xff),
    .bg_button = LV_COLOR_MAKE(0x4a, 0x90, 0xd9),
    .bg_button_pressed = LV_COLOR_MAKE(0x3a, 0x80, 0xc9),
    .text_primary = LV_COLOR_MAKE(0x33, 0x33, 0x33),
    .text_secondary = LV_COLOR_MAKE(0x66, 0x66, 0x66),
    .text_highlight = LV_COLOR_MAKE(0x00, 0x66, 0xcc),
    .border = LV_COLOR_MAKE(0xdd, 0xdd, 0xdd),
    .accent = LV_COLOR_MAKE(0x4a, 0x90, 0xd9),
};

static void apply_theme_to_screen(lv_obj_t *scr)
{
    const theme_colors_t *colors = theme_manager_get_colors();
    
    lv_obj_set_style_bg_color(scr, colors->bg_primary, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
}

static void apply_theme_to_button(lv_obj_t *btn)
{
    const theme_colors_t *colors = theme_manager_get_colors();
    
    lv_obj_set_style_bg_color(btn, colors->bg_button, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, colors->bg_button_pressed, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
    
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    if (label && lv_obj_check_type(label, &lv_label_class)) {
        lv_obj_set_style_text_color(label, colors->text_primary, LV_PART_MAIN);
    }
}

static void apply_theme_to_label(lv_obj_t *label)
{
    const theme_colors_t *colors = theme_manager_get_colors();
    lv_obj_set_style_text_color(label, colors->text_primary, LV_PART_MAIN);
}

static void apply_theme_recursive(lv_obj_t *obj)
{
    const theme_colors_t *colors = theme_manager_get_colors();
    
    if (lv_obj_check_type(obj, &lv_button_class)) {
        apply_theme_to_button(obj);
    } else if (lv_obj_check_type(obj, &lv_label_class)) {
        apply_theme_to_label(obj);
    } else if (lv_obj_check_type(obj, &lv_slider_class)) {
        lv_obj_set_style_bg_color(obj, colors->bg_card, LV_PART_MAIN);
        lv_obj_set_style_bg_color(obj, colors->accent, LV_PART_INDICATOR);
        lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    } else if (lv_obj_check_type(obj, &lv_list_class)) {
        lv_obj_set_style_bg_color(obj, colors->bg_card, LV_PART_MAIN);
    } else if (lv_obj_check_type(obj, &lv_image_class)) {
        ;
    } else {
        lv_obj_set_style_bg_color(obj, colors->bg_card, LV_PART_MAIN);
        lv_obj_set_style_border_color(obj, colors->border, LV_PART_MAIN);
    }
    
    uint32_t child_count = lv_obj_get_child_cnt(obj);
    for (uint32_t i = 0; i < child_count; i++) {
        apply_theme_recursive(lv_obj_get_child(obj, i));
    }
}

esp_err_t theme_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing theme manager...");
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("system", NVS_READWRITE, &nvs_handle);
    if (ret == ESP_OK) {
        int32_t mode = THEME_MODE_DARK;
        ret = nvs_get_i32(nvs_handle, "theme_mode", &mode);
        if (ret == ESP_OK) {
            s_current_mode = (theme_mode_t)mode;
            ESP_LOGI(TAG, "Loaded theme mode from NVS: %d", s_current_mode);
        } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
            s_current_mode = THEME_MODE_DARK;
            ESP_LOGI(TAG, "Theme mode not found in NVS, using default: dark");
        }
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    theme_manager_apply_global();
    ESP_LOGI(TAG, "Theme manager initialized");
    return ESP_OK;
}

void theme_manager_set_mode(theme_mode_t mode)
{
    if (s_current_mode == mode) return;
    
    s_current_mode = mode;
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("system", NVS_READWRITE, &nvs_handle);
    if (ret == ESP_OK) {
        nvs_set_i32(nvs_handle, "theme_mode", (int32_t)mode);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
    
    theme_manager_apply_global();
    
    ESP_LOGI(TAG, "Theme mode changed to: %s", 
             mode == THEME_MODE_DARK ? "dark" : "light");
}

theme_mode_t theme_manager_get_mode(void)
{
    return s_current_mode;
}

void theme_manager_apply(lv_obj_t *obj)
{
    if (!obj) return;
    apply_theme_recursive(obj);
}

void theme_manager_apply_global(void)
{
    lv_obj_t *active_scr = lv_screen_active();
    if (active_scr) {
        apply_theme_to_screen(active_scr);
        apply_theme_recursive(active_scr);
    }
}

const theme_colors_t *theme_manager_get_colors(void)
{
    return s_current_mode == THEME_MODE_DARK ? &s_dark_colors : &s_light_colors;
}