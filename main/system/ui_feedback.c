#include "ui_feedback.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "UI_FEEDBACK";

static lv_obj_t *toast_container = NULL;
static lv_obj_t *loading_overlay = NULL;
static lv_obj_t *error_dialog = NULL;
static lv_timer_t *toast_timer = NULL;

static lv_color_t get_feedback_color(ui_feedback_type_t type)
{
    switch (type) {
        case UI_FEEDBACK_TYPE_INFO: return lv_color_hex(0x4a90d9);
        case UI_FEEDBACK_TYPE_SUCCESS: return lv_color_hex(0x4CAF50);
        case UI_FEEDBACK_TYPE_WARNING: return lv_color_hex(0xFFA500);
        case UI_FEEDBACK_TYPE_ERROR: return lv_color_hex(0xFF4444);
        default: return lv_color_hex(0x4a90d9);
    }
}

static const char *get_feedback_icon(ui_feedback_type_t type)
{
    switch (type) {
        case UI_FEEDBACK_TYPE_INFO: return "ℹ️";
        case UI_FEEDBACK_TYPE_SUCCESS: return "✅";
        case UI_FEEDBACK_TYPE_WARNING: return "⚠️";
        case UI_FEEDBACK_TYPE_ERROR: return "❌";
        default: return "📌";
    }
}

static void toast_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    
    if (toast_container) {
        lv_obj_del(toast_container);
        toast_container = NULL;
    }
    
    if (toast_timer) {
        lv_timer_del(toast_timer);
        toast_timer = NULL;
    }
}

static void toast_anim_exec_cb(void *obj, int32_t value)
{
    lv_obj_set_style_opa(obj, value, 0);
}

esp_err_t ui_feedback_init(void)
{
    ESP_LOGI(TAG, "UI feedback initialized");
    return ESP_OK;
}

void ui_feedback_show_toast(const char *title, const char *message, ui_feedback_type_t type, uint32_t duration_ms)
{
    if (toast_timer) {
        lv_timer_del(toast_timer);
        toast_timer = NULL;
    }
    
    if (toast_container) {
        lv_obj_del(toast_container);
        toast_container = NULL;
    }
    
    lv_obj_t *scr = lv_scr_act();
    if (!scr) return;
    
    toast_container = lv_obj_create(scr);
    lv_obj_set_size(toast_container, 360, 80);
    lv_obj_set_style_bg_color(toast_container, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_color(toast_container, get_feedback_color(type), 0);
    lv_obj_set_style_border_width(toast_container, 2, 0);
    lv_obj_set_style_radius(toast_container, 10, 0);
    lv_obj_set_style_shadow_width(toast_container, 10, 0);
    lv_obj_set_style_shadow_color(toast_container, lv_color_hex(0x000000), 0);
    lv_obj_align(toast_container, LV_ALIGN_TOP_MID, 0, 50);
    
    lv_obj_t *icon_label = lv_label_create(toast_container);
    lv_label_set_text(icon_label, get_feedback_icon(type));
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_14, 0);
    lv_obj_align(icon_label, LV_ALIGN_LEFT_MID, 15, 0);
    
    lv_obj_t *title_label = lv_label_create(toast_container);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 50, 10);
    
    lv_obj_t *msg_label = lv_label_create(toast_container);
    lv_label_set_text(msg_label, message);
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(msg_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(msg_label, LV_ALIGN_BOTTOM_LEFT, 50, -10);
    
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, toast_container);
    lv_anim_set_values(&anim, 0, 255);
    lv_anim_set_time(&anim, 200);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)toast_anim_exec_cb);
    lv_anim_start(&anim);
    
    toast_timer = lv_timer_create(toast_timer_cb, duration_ms, NULL);
}

void ui_feedback_show_loading(lv_obj_t *parent, const char *text)
{
    if (loading_overlay) {
        lv_obj_del(loading_overlay);
        loading_overlay = NULL;
    }
    
    if (!parent) parent = lv_scr_act();
    if (!parent) return;
    
    loading_overlay = lv_obj_create(parent);
    lv_obj_set_size(loading_overlay, 200, 120);
    lv_obj_set_style_bg_color(loading_overlay, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(loading_overlay, 1, 0);
    lv_obj_set_style_border_color(loading_overlay, lv_color_hex(0x4a8a6a), 0);
    lv_obj_set_style_radius(loading_overlay, 10, 0);
    lv_obj_set_style_opa(loading_overlay, 240, 0);
    lv_obj_center(loading_overlay);
    
    lv_obj_t *spinner = lv_spinner_create(loading_overlay);
    lv_obj_set_size(spinner, 40, 40);
    lv_obj_set_style_line_color(spinner, lv_color_hex(0x4a8a6a), 0);
    lv_obj_align(spinner, LV_ALIGN_TOP_MID, 0, 20);
    
    lv_obj_t *label = lv_label_create(loading_overlay);
    lv_label_set_text(label, text ? text : "Loading...");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -15);
}

void ui_feedback_hide_loading(void)
{
    if (loading_overlay) {
        lv_obj_del(loading_overlay);
        loading_overlay = NULL;
    }
}

static void error_dialog_close_cb(lv_event_t *e)
{
    (void)e;
    
    if (error_dialog) {
        lv_obj_del(error_dialog);
        error_dialog = NULL;
    }
}

void ui_feedback_show_error(const char *title, const char *message)
{
    if (error_dialog) {
        lv_obj_del(error_dialog);
        error_dialog = NULL;
    }
    
    lv_obj_t *scr = lv_scr_act();
    if (!scr) return;
    
    error_dialog = lv_obj_create(scr);
    lv_obj_set_size(error_dialog, 360, 200);
    lv_obj_set_style_bg_color(error_dialog, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_color(error_dialog, lv_color_hex(0xFF4444), 0);
    lv_obj_set_style_border_width(error_dialog, 2, 0);
    lv_obj_set_style_radius(error_dialog, 10, 0);
    lv_obj_center(error_dialog);
    
    lv_obj_t *icon_label = lv_label_create(error_dialog);
    lv_label_set_text(icon_label, "❌");
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_14, 0);
    lv_obj_align(icon_label, LV_ALIGN_TOP_MID, 0, 20);
    
    lv_obj_t *title_label = lv_label_create(error_dialog);
    lv_label_set_text(title_label, title ? title : "Error");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFF4444), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 50);
    
    lv_obj_t *msg_label = lv_label_create(error_dialog);
    lv_label_set_text(msg_label, message ? message : "An error occurred");
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(msg_label, 320, 60);
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(msg_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(msg_label, LV_ALIGN_TOP_MID, 0, 80);
    
    lv_obj_t *close_btn = lv_btn_create(error_dialog);
    lv_obj_set_size(close_btn, 120, 40);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xFF4444), 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xFF6666), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(close_btn, 0, 0);
    lv_obj_set_style_radius(close_btn, 8, 0);
    lv_obj_align(close_btn, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_add_event_cb(close_btn, error_dialog_close_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *btn_label = lv_label_create(close_btn);
    lv_label_set_text(btn_label, "OK");
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(btn_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(btn_label);
}

static void button_scale_anim_exec_cb(void *obj, int32_t value)
{
    lv_obj_set_style_transform_scale(obj, value, 0);
}

void ui_feedback_button_press(lv_obj_t *btn)
{
    if (!btn) return;
    
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, btn);
    lv_anim_set_values(&anim, 256, 220);
    lv_anim_set_time(&anim, 100);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)button_scale_anim_exec_cb);
    lv_anim_start(&anim);
    
    lv_anim_t anim2;
    lv_anim_init(&anim2);
    lv_anim_set_var(&anim2, btn);
    lv_anim_set_values(&anim2, 220, 256);
    lv_anim_set_time(&anim2, 100);
    lv_anim_set_path_cb(&anim2, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&anim2, (lv_anim_exec_xcb_t)button_scale_anim_exec_cb);
    lv_anim_set_delay(&anim2, 100);
    lv_anim_start(&anim2);
}