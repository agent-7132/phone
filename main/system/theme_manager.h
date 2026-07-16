#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    THEME_MODE_DARK,
    THEME_MODE_LIGHT
} theme_mode_t;

typedef struct {
    lv_color_t bg_primary;
    lv_color_t bg_secondary;
    lv_color_t bg_card;
    lv_color_t bg_button;
    lv_color_t bg_button_pressed;
    lv_color_t text_primary;
    lv_color_t text_secondary;
    lv_color_t text_highlight;
    lv_color_t border;
    lv_color_t accent;
} theme_colors_t;

esp_err_t theme_manager_init(void);
void theme_manager_set_mode(theme_mode_t mode);
theme_mode_t theme_manager_get_mode(void);
void theme_manager_apply(lv_obj_t *obj);
void theme_manager_apply_global(void);
const theme_colors_t *theme_manager_get_colors(void);

#ifdef __cplusplus
}
#endif