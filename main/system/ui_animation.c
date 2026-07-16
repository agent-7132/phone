#include "ui_animation.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "UI_ANIMATION";

static uint32_t default_duration = 300;
static ui_anim_ease_t default_ease = UI_ANIM_EASE_OUT;

static lv_anim_path_cb_t get_ease_func(ui_anim_ease_t ease)
{
    switch (ease) {
        case UI_ANIM_EASE_IN:
            return lv_anim_path_ease_in;
        case UI_ANIM_EASE_OUT:
            return lv_anim_path_ease_out;
        case UI_ANIM_EASE_IN_OUT:
            return lv_anim_path_ease_in_out;
        case UI_ANIM_EASE_LINEAR:
        default:
            return lv_anim_path_linear;
    }
}

static void anim_set_opa_cb(void *obj, int32_t value)
{
    lv_obj_set_style_opa(obj, value, LV_PART_MAIN);
}

static void anim_set_x_cb(void *obj, int32_t value)
{
    lv_obj_set_x(obj, value);
}

static void anim_set_y_cb(void *obj, int32_t value)
{
    lv_obj_set_y(obj, value);
}

static void anim_set_scale_cb(void *obj, int32_t value)
{
    lv_obj_set_style_transform_scale(obj, value, LV_PART_MAIN);
}

static void anim_set_angle_cb(void *obj, int32_t value)
{
    lv_obj_set_style_transform_angle(obj, value, LV_PART_MAIN);
}

esp_err_t ui_animation_init(void)
{
    ESP_LOGI(TAG, "Initializing UI animation...");
    ESP_LOGI(TAG, "UI animation initialized");
    return ESP_OK;
}

void ui_animation_fade(lv_obj_t *obj, bool fade_in, uint32_t duration)
{
    if (!obj) {
        return;
    }
    
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_exec_cb(&anim, anim_set_opa_cb);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, fade_in ? 0 : 255, fade_in ? 255 : 0);
    lv_anim_set_time(&anim, duration);
    lv_anim_set_path_cb(&anim, get_ease_func(default_ease));
    lv_anim_start(&anim);
}

void ui_animation_slide(lv_obj_t *obj, lv_dir_t dir, uint32_t duration)
{
    if (!obj) {
        return;
    }
    
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    
    lv_coord_t w = lv_obj_get_width(obj);
    lv_coord_t h = lv_obj_get_height(obj);
    
    lv_coord_t start_x = lv_obj_get_x(obj);
    lv_coord_t start_y = lv_obj_get_y(obj);
    
    lv_coord_t end_x = start_x;
    lv_coord_t end_y = start_y;
    
    switch (dir) {
        case LV_DIR_LEFT:
            start_x = w;
            end_x = 0;
            break;
        case LV_DIR_RIGHT:
            start_x = -w;
            end_x = 0;
            break;
        case LV_DIR_TOP:
            start_y = h;
            end_y = 0;
            break;
        case LV_DIR_BOTTOM:
            start_y = -h;
            end_y = 0;
            break;
        case LV_DIR_NONE:
        case LV_DIR_HOR:
        case LV_DIR_VER:
        case LV_DIR_ALL:
        default:
            break;
    }
    
    lv_anim_t anim_x;
    lv_anim_init(&anim_x);
    lv_anim_set_exec_cb(&anim_x, anim_set_x_cb);
    lv_anim_set_var(&anim_x, obj);
    lv_anim_set_values(&anim_x, start_x, end_x);
    lv_anim_set_time(&anim_x, duration);
    lv_anim_set_path_cb(&anim_x, get_ease_func(default_ease));
    lv_anim_start(&anim_x);
    
    if (dir == LV_DIR_TOP || dir == LV_DIR_BOTTOM) {
        lv_anim_t anim_y;
        lv_anim_init(&anim_y);
        lv_anim_set_exec_cb(&anim_y, anim_set_y_cb);
        lv_anim_set_var(&anim_y, obj);
        lv_anim_set_values(&anim_y, start_y, end_y);
        lv_anim_set_time(&anim_y, duration);
        lv_anim_set_path_cb(&anim_y, get_ease_func(default_ease));
        lv_anim_start(&anim_y);
    }
}

void ui_animation_scale(lv_obj_t *obj, bool scale_in, uint32_t duration)
{
    if (!obj) {
        return;
    }
    
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_exec_cb(&anim, anim_set_scale_cb);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, scale_in ? 0 : 256, scale_in ? 256 : 0);
    lv_anim_set_time(&anim, duration);
    lv_anim_set_path_cb(&anim, get_ease_func(default_ease));
    lv_anim_start(&anim);
}

void ui_animation_bounce(lv_obj_t *obj, bool bounce_in, uint32_t duration)
{
    if (!obj) {
        return;
    }
    
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_exec_cb(&anim, anim_set_scale_cb);
    lv_anim_set_var(&anim, obj);
    
    if (bounce_in) {
        lv_anim_set_values(&anim, 0, 256);
        lv_anim_set_path_cb(&anim, lv_anim_path_overshoot);
    } else {
        lv_anim_set_values(&anim, 256, 0);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_in);
    }
    
    lv_anim_set_time(&anim, duration);
    lv_anim_start(&anim);
}

void ui_animation_rotate(lv_obj_t *obj, bool rotate_in, uint32_t duration)
{
    if (!obj) {
        return;
    }
    
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_exec_cb(&anim, anim_set_angle_cb);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, rotate_in ? -1800 : 0, rotate_in ? 0 : 1800);
    lv_anim_set_time(&anim, duration);
    lv_anim_set_path_cb(&anim, get_ease_func(default_ease));
    lv_anim_start(&anim);
}

void ui_animation_start(lv_obj_t *obj, ui_anim_type_t type, uint32_t duration, 
                        ui_anim_ease_t ease, ui_anim_done_cb_t cb)
{
    if (!obj) {
        return;
    }
    
    ui_anim_ease_t old_ease = default_ease;
    if (ease != UI_ANIM_EASE_LINEAR) {
        default_ease = ease;
    }
    
    switch (type) {
        case UI_ANIM_FADE_IN:
            ui_animation_fade(obj, true, duration);
            break;
        case UI_ANIM_FADE_OUT:
            ui_animation_fade(obj, false, duration);
            break;
        case UI_ANIM_SLIDE_LEFT:
            ui_animation_slide(obj, LV_DIR_LEFT, duration);
            break;
        case UI_ANIM_SLIDE_RIGHT:
            ui_animation_slide(obj, LV_DIR_RIGHT, duration);
            break;
        case UI_ANIM_SLIDE_UP:
            ui_animation_slide(obj, LV_DIR_TOP, duration);
            break;
        case UI_ANIM_SLIDE_DOWN:
            ui_animation_slide(obj, LV_DIR_BOTTOM, duration);
            break;
        case UI_ANIM_SCALE_IN:
            ui_animation_scale(obj, true, duration);
            break;
        case UI_ANIM_SCALE_OUT:
            ui_animation_scale(obj, false, duration);
            break;
        case UI_ANIM_BOUNCE_IN:
            ui_animation_bounce(obj, true, duration);
            break;
        case UI_ANIM_ROTATE_IN:
            ui_animation_rotate(obj, true, duration);
            break;
    }
    
    default_ease = old_ease;
    
    if (cb) {
        vTaskDelay(pdMS_TO_TICKS(duration));
        cb(obj);
    }
}

void ui_animation_stop(lv_obj_t *obj)
{
    if (!obj) {
        return;
    }
    
    lv_anim_del(obj, NULL);
}

void ui_animation_set_default_duration(uint32_t duration)
{
    default_duration = duration;
}

void ui_animation_set_default_ease(ui_anim_ease_t ease)
{
    default_ease = ease;
}

uint32_t ui_animation_get_default_duration(void)
{
    return default_duration;
}

ui_anim_ease_t ui_animation_get_default_ease(void)
{
    return default_ease;
}