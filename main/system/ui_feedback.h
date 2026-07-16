#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_FEEDBACK_TYPE_INFO,
    UI_FEEDBACK_TYPE_SUCCESS,
    UI_FEEDBACK_TYPE_WARNING,
    UI_FEEDBACK_TYPE_ERROR
} ui_feedback_type_t;

esp_err_t ui_feedback_init(void);

void ui_feedback_show_toast(const char *title, const char *message, ui_feedback_type_t type, uint32_t duration_ms);

void ui_feedback_show_loading(lv_obj_t *parent, const char *text);

void ui_feedback_hide_loading(void);

void ui_feedback_show_error(const char *title, const char *message);

void ui_feedback_button_press(lv_obj_t *btn);

#ifdef __cplusplus
}
#endif