#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_ANIM_FADE_IN,
    UI_ANIM_FADE_OUT,
    UI_ANIM_SLIDE_LEFT,
    UI_ANIM_SLIDE_RIGHT,
    UI_ANIM_SLIDE_UP,
    UI_ANIM_SLIDE_DOWN,
    UI_ANIM_SCALE_IN,
    UI_ANIM_SCALE_OUT,
    UI_ANIM_BOUNCE_IN,
    UI_ANIM_ROTATE_IN
} ui_anim_type_t;

typedef enum {
    UI_ANIM_EASE_LINEAR,
    UI_ANIM_EASE_IN,
    UI_ANIM_EASE_OUT,
    UI_ANIM_EASE_IN_OUT
} ui_anim_ease_t;

typedef void (*ui_anim_done_cb_t)(lv_obj_t *obj);

esp_err_t ui_animation_init(void);

void ui_animation_start(lv_obj_t *obj, ui_anim_type_t type, uint32_t duration, 
                        ui_anim_ease_t ease, ui_anim_done_cb_t cb);

void ui_animation_fade(lv_obj_t *obj, bool fade_in, uint32_t duration);

void ui_animation_slide(lv_obj_t *obj, lv_dir_t dir, uint32_t duration);

void ui_animation_scale(lv_obj_t *obj, bool scale_in, uint32_t duration);

void ui_animation_bounce(lv_obj_t *obj, bool bounce_in, uint32_t duration);

void ui_animation_rotate(lv_obj_t *obj, bool rotate_in, uint32_t duration);

void ui_animation_stop(lv_obj_t *obj);

void ui_animation_set_default_duration(uint32_t duration);

void ui_animation_set_default_ease(ui_anim_ease_t ease);

uint32_t ui_animation_get_default_duration(void);

ui_anim_ease_t ui_animation_get_default_ease(void);

#ifdef __cplusplus
}
#endif