#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_UTILS_COLOR_PRIMARY_BG 0x1a1a2e
#define UI_UTILS_COLOR_SECONDARY_BG 0x252540
#define UI_UTILS_COLOR_CARD_BG 0x252540
#define UI_UTILS_COLOR_BUTTON_BG 0x3a3a5a
#define UI_UTILS_COLOR_BUTTON_PRESSED 0x4a4a6a
#define UI_UTILS_COLOR_BUTTON_ACTIVE 0x4a4a6a
#define UI_UTILS_COLOR_TEXT_PRIMARY 0xFFFFFF
#define UI_UTILS_COLOR_TEXT_SECONDARY 0xAAAAAA
#define UI_UTILS_COLOR_BORDER 0x3a3a5a
#define UI_UTILS_COLOR_ACCENT 0x6c5ce7

#define UI_UTILS_FONT_DEFAULT &lv_font_montserrat_14
#define UI_UTILS_FONT_TITLE &lv_font_montserrat_14
#define UI_UTILS_FONT_SMALL &lv_font_montserrat_14

#define UI_UTILS_BORDER_WIDTH 1
#define UI_UTILS_RADIUS_DEFAULT 10
#define UI_UTILS_RADIUS_BUTTON 8
#define UI_UTILS_RADIUS_CARD 10

#define UI_UTILS_PADDING_SMALL 5
#define UI_UTILS_PADDING_DEFAULT 10
#define UI_UTILS_PADDING_LARGE 15

typedef struct {
    lv_color_t bg_color;
    lv_color_t border_color;
    uint8_t border_width;
    uint8_t radius;
    uint8_t padding;
} ui_container_style_t;

typedef struct {
    lv_color_t normal_color;
    lv_color_t pressed_color;
    lv_color_t active_color;
    lv_color_t text_color;
    const lv_font_t *font;
    uint8_t radius;
    uint16_t width;
    uint16_t height;
} ui_button_style_t;

typedef struct {
    lv_color_t bg_color;
    lv_color_t accent_color;
    lv_color_t text_color;
    const lv_font_t *font;
    uint16_t width;
    uint16_t height;
    int32_t min;
    int32_t max;
    int32_t value;
} ui_slider_style_t;

typedef struct {
    lv_color_t bg_color;
    lv_color_t accent_color;
    lv_color_t text_color;
    const lv_font_t *font;
    uint16_t width;
    uint16_t height;
    bool checked;
} ui_checkbox_style_t;

typedef struct {
    lv_color_t bg_color;
    lv_color_t accent_color;
    uint16_t width;
    uint16_t height;
    uint8_t radius;
    int32_t min;
    int32_t max;
    int32_t value;
} ui_progress_style_t;

typedef struct {
    lv_color_t bg_color;
    lv_color_t border_color;
    lv_color_t text_color;
    const lv_font_t *font;
    uint16_t width;
    uint16_t height;
    uint8_t radius;
    const char *placeholder;
} ui_input_style_t;

lv_obj_t *ui_utils_create_screen(void);
lv_obj_t *ui_utils_create_container(lv_obj_t *parent, uint16_t width, uint16_t height, 
                                    const ui_container_style_t *style);
lv_obj_t *ui_utils_create_container_default(lv_obj_t *parent, uint16_t width, uint16_t height);
lv_obj_t *ui_utils_create_button(lv_obj_t *parent, const char *text, 
                                 lv_event_cb_t callback, void *user_data,
                                 const ui_button_style_t *style);
lv_obj_t *ui_utils_create_button_default(lv_obj_t *parent, const char *text, 
                                         lv_event_cb_t callback, void *user_data);
lv_obj_t *ui_utils_create_back_button(lv_obj_t *parent);
lv_obj_t *ui_utils_create_title(lv_obj_t *parent, const char *text);
lv_obj_t *ui_utils_create_label(lv_obj_t *parent, const char *text, 
                                const lv_font_t *font, lv_color_t color);
lv_obj_t *ui_utils_create_label_default(lv_obj_t *parent, const char *text);
void ui_utils_set_button_text(lv_obj_t *btn, const char *text);
void ui_utils_set_button_style(lv_obj_t *btn, const ui_button_style_t *style);
void ui_utils_center_label(lv_obj_t *btn);

lv_obj_t *ui_utils_create_slider(lv_obj_t *parent, lv_event_cb_t callback, void *user_data,
                                 const ui_slider_style_t *style);
lv_obj_t *ui_utils_create_slider_default(lv_obj_t *parent, lv_event_cb_t callback, void *user_data);
int32_t ui_utils_get_slider_value(lv_obj_t *slider);
void ui_utils_set_slider_value(lv_obj_t *slider, int32_t value);

lv_obj_t *ui_utils_create_checkbox(lv_obj_t *parent, const char *text, lv_event_cb_t callback,
                                   void *user_data, const ui_checkbox_style_t *style);
lv_obj_t *ui_utils_create_checkbox_default(lv_obj_t *parent, const char *text,
                                           lv_event_cb_t callback, void *user_data);
bool ui_utils_get_checkbox_value(lv_obj_t *checkbox);
void ui_utils_set_checkbox_value(lv_obj_t *checkbox, bool checked);

lv_obj_t *ui_utils_create_progress(lv_obj_t *parent, const ui_progress_style_t *style);
lv_obj_t *ui_utils_create_progress_default(lv_obj_t *parent);
int32_t ui_utils_get_progress_value(lv_obj_t *progress);
void ui_utils_set_progress_value(lv_obj_t *progress, int32_t value);

lv_obj_t *ui_utils_create_input(lv_obj_t *parent, lv_event_cb_t callback, void *user_data,
                                const ui_input_style_t *style);
lv_obj_t *ui_utils_create_input_default(lv_obj_t *parent, lv_event_cb_t callback, void *user_data);
void ui_utils_set_input_text(lv_obj_t *input, const char *text);
const char *ui_utils_get_input_text(lv_obj_t *input);

#ifdef __cplusplus
}
#endif