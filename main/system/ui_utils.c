#include "ui_utils.h"
#include "app_manager.h"

static void back_button_cb(lv_event_t *e)
{
    app_manager_go_home();
}

lv_obj_t *ui_utils_create_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_UTILS_COLOR_PRIMARY_BG), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    return scr;
}

lv_obj_t *ui_utils_create_container(lv_obj_t *parent, uint16_t width, uint16_t height,
                                    const ui_container_style_t *style)
{
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, width, height);
    
    if (style) {
        lv_obj_set_style_bg_color(container, style->bg_color, 0);
        lv_obj_set_style_border_width(container, style->border_width, 0);
        lv_obj_set_style_border_color(container, style->border_color, 0);
        lv_obj_set_style_radius(container, style->radius, 0);
        lv_obj_set_style_pad_all(container, style->padding, 0);
    } else {
        lv_obj_set_style_bg_color(container, lv_color_hex(UI_UTILS_COLOR_CARD_BG), 0);
        lv_obj_set_style_border_width(container, UI_UTILS_BORDER_WIDTH, 0);
        lv_obj_set_style_border_color(container, lv_color_hex(UI_UTILS_COLOR_BORDER), 0);
        lv_obj_set_style_radius(container, UI_UTILS_RADIUS_CARD, 0);
        lv_obj_set_style_pad_all(container, UI_UTILS_PADDING_DEFAULT, 0);
    }
    
    return container;
}

lv_obj_t *ui_utils_create_container_default(lv_obj_t *parent, uint16_t width, uint16_t height)
{
    return ui_utils_create_container(parent, width, height, NULL);
}

lv_obj_t *ui_utils_create_button(lv_obj_t *parent, const char *text,
                                 lv_event_cb_t callback, void *user_data,
                                 const ui_button_style_t *style)
{
    lv_obj_t *btn = lv_btn_create(parent);
    
    if (style) {
        lv_obj_set_size(btn, style->width, style->height);
        lv_obj_set_style_bg_color(btn, style->normal_color, 0);
        lv_obj_set_style_bg_color(btn, style->pressed_color, LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(btn, style->active_color, LV_STATE_CHECKED);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, style->radius, 0);
    } else {
        lv_obj_set_size(btn, 100, 40);
        lv_obj_set_style_bg_color(btn, lv_color_hex(UI_UTILS_COLOR_BUTTON_BG), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(UI_UTILS_COLOR_BUTTON_PRESSED), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, UI_UTILS_RADIUS_BUTTON, 0);
    }
    
    if (callback) {
        lv_obj_add_event_cb(btn, callback, LV_EVENT_CLICKED, user_data);
    }
    
    if (text) {
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, text);
        
        if (style && style->font) {
            lv_obj_set_style_text_font(label, style->font, 0);
            lv_obj_set_style_text_color(label, style->text_color, 0);
        } else {
            lv_obj_set_style_text_font(label, UI_UTILS_FONT_DEFAULT, 0);
            lv_obj_set_style_text_color(label, lv_color_hex(UI_UTILS_COLOR_TEXT_PRIMARY), 0);
        }
        
        lv_obj_center(label);
    }
    
    return btn;
}

lv_obj_t *ui_utils_create_button_default(lv_obj_t *parent, const char *text,
                                         lv_event_cb_t callback, void *user_data)
{
    return ui_utils_create_button(parent, text, callback, user_data, NULL);
}

lv_obj_t *ui_utils_create_back_button(lv_obj_t *parent)
{
    lv_obj_t *btn = ui_utils_create_button(parent, "Back", back_button_cb, NULL, NULL);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    return btn;
}

lv_obj_t *ui_utils_create_title(lv_obj_t *parent, const char *text)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, text);
    lv_obj_set_style_text_font(title, UI_UTILS_FONT_TITLE, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(UI_UTILS_COLOR_TEXT_PRIMARY), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);
    return title;
}

lv_obj_t *ui_utils_create_label(lv_obj_t *parent, const char *text,
                                const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    
    if (font) {
        lv_obj_set_style_text_font(label, font, 0);
    } else {
        lv_obj_set_style_text_font(label, UI_UTILS_FONT_DEFAULT, 0);
    }
    
    lv_obj_set_style_text_color(label, color, 0);
    
    return label;
}

lv_obj_t *ui_utils_create_label_default(lv_obj_t *parent, const char *text)
{
    return ui_utils_create_label(parent, text, UI_UTILS_FONT_DEFAULT, 
                                 lv_color_hex(UI_UTILS_COLOR_TEXT_PRIMARY));
}

void ui_utils_set_button_text(lv_obj_t *btn, const char *text)
{
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    if (label && lv_obj_check_type(label, &lv_label_class)) {
        lv_label_set_text(label, text);
    }
}

void ui_utils_set_button_style(lv_obj_t *btn, const ui_button_style_t *style)
{
    if (!btn || !style) return;
    
    lv_obj_set_size(btn, style->width, style->height);
    lv_obj_set_style_bg_color(btn, style->normal_color, 0);
    lv_obj_set_style_bg_color(btn, style->pressed_color, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, style->active_color, LV_STATE_CHECKED);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, style->radius, 0);
    
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    if (label && lv_obj_check_type(label, &lv_label_class)) {
        lv_obj_set_style_text_font(label, style->font, 0);
        lv_obj_set_style_text_color(label, style->text_color, 0);
    }
}

void ui_utils_center_label(lv_obj_t *btn)
{
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    if (label) {
        lv_obj_center(label);
    }
}

lv_obj_t *ui_utils_create_slider(lv_obj_t *parent, lv_event_cb_t callback, void *user_data,
                                 const ui_slider_style_t *style)
{
    lv_obj_t *slider = lv_slider_create(parent);
    
    if (style) {
        lv_obj_set_size(slider, style->width, style->height);
        lv_obj_set_style_bg_color(slider, style->bg_color, 0);
        lv_obj_set_style_bg_color(slider, style->accent_color, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider, style->accent_color, LV_PART_KNOB);
        lv_slider_set_range(slider, style->min, style->max);
        lv_slider_set_value(slider, style->value, LV_ANIM_OFF);
    } else {
        lv_obj_set_size(slider, 200, 30);
        lv_obj_set_style_bg_color(slider, lv_color_hex(UI_UTILS_COLOR_CARD_BG), 0);
        lv_obj_set_style_bg_color(slider, lv_color_hex(UI_UTILS_COLOR_ACCENT), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider, lv_color_hex(UI_UTILS_COLOR_ACCENT), LV_PART_KNOB);
        lv_slider_set_range(slider, 0, 100);
        lv_slider_set_value(slider, 50, LV_ANIM_OFF);
    }
    
    if (callback) {
        lv_obj_add_event_cb(slider, callback, LV_EVENT_VALUE_CHANGED, user_data);
    }
    
    return slider;
}

lv_obj_t *ui_utils_create_slider_default(lv_obj_t *parent, lv_event_cb_t callback, void *user_data)
{
    return ui_utils_create_slider(parent, callback, user_data, NULL);
}

int32_t ui_utils_get_slider_value(lv_obj_t *slider)
{
    if (!slider) return 0;
    return lv_slider_get_value(slider);
}

void ui_utils_set_slider_value(lv_obj_t *slider, int32_t value)
{
    if (!slider) return;
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
}

lv_obj_t *ui_utils_create_checkbox(lv_obj_t *parent, const char *text, lv_event_cb_t callback,
                                   void *user_data, const ui_checkbox_style_t *style)
{
    lv_obj_t *checkbox = lv_checkbox_create(parent);
    
    if (style) {
        lv_obj_set_size(checkbox, style->width, style->height);
        lv_obj_set_style_bg_color(checkbox, style->bg_color, 0);
        lv_obj_set_style_bg_color(checkbox, style->accent_color, LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(checkbox, style->accent_color, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(checkbox, style->accent_color, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(checkbox, style->text_color, 0);
        if (style->font) {
            lv_obj_set_style_text_font(checkbox, style->font, 0);
        }
        if (style->checked) {
            lv_obj_add_state(checkbox, LV_STATE_CHECKED);
        }
    } else {
        lv_obj_set_size(checkbox, 200, 40);
        lv_obj_set_style_bg_color(checkbox, lv_color_hex(UI_UTILS_COLOR_CARD_BG), 0);
        lv_obj_set_style_bg_color(checkbox, lv_color_hex(UI_UTILS_COLOR_ACCENT), LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(checkbox, lv_color_hex(UI_UTILS_COLOR_ACCENT), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(checkbox, lv_color_hex(UI_UTILS_COLOR_ACCENT), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(checkbox, lv_color_hex(UI_UTILS_COLOR_TEXT_PRIMARY), 0);
        lv_obj_set_style_text_font(checkbox, UI_UTILS_FONT_DEFAULT, 0);
    }
    
    if (text) {
        lv_checkbox_set_text(checkbox, text);
    }
    
    if (callback) {
        lv_obj_add_event_cb(checkbox, callback, LV_EVENT_VALUE_CHANGED, user_data);
    }
    
    return checkbox;
}

lv_obj_t *ui_utils_create_checkbox_default(lv_obj_t *parent, const char *text,
                                           lv_event_cb_t callback, void *user_data)
{
    return ui_utils_create_checkbox(parent, text, callback, user_data, NULL);
}

bool ui_utils_get_checkbox_value(lv_obj_t *checkbox)
{
    if (!checkbox) return false;
    return lv_obj_has_state(checkbox, LV_STATE_CHECKED);
}

void ui_utils_set_checkbox_value(lv_obj_t *checkbox, bool checked)
{
    if (!checkbox) return;
    if (checked) {
        lv_obj_add_state(checkbox, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(checkbox, LV_STATE_CHECKED);
    }
}

lv_obj_t *ui_utils_create_progress(lv_obj_t *parent, const ui_progress_style_t *style)
{
    lv_obj_t *progress = lv_bar_create(parent);
    
    if (style) {
        lv_obj_set_size(progress, style->width, style->height);
        lv_obj_set_style_bg_color(progress, style->bg_color, 0);
        lv_obj_set_style_bg_color(progress, style->accent_color, LV_PART_INDICATOR);
        lv_obj_set_style_radius(progress, style->radius, 0);
        lv_bar_set_range(progress, style->min, style->max);
        lv_bar_set_value(progress, style->value, LV_ANIM_OFF);
    } else {
        lv_obj_set_size(progress, 200, 20);
        lv_obj_set_style_bg_color(progress, lv_color_hex(UI_UTILS_COLOR_CARD_BG), 0);
        lv_obj_set_style_bg_color(progress, lv_color_hex(UI_UTILS_COLOR_ACCENT), LV_PART_INDICATOR);
        lv_obj_set_style_radius(progress, UI_UTILS_RADIUS_DEFAULT, 0);
        lv_bar_set_range(progress, 0, 100);
        lv_bar_set_value(progress, 0, LV_ANIM_OFF);
    }
    
    return progress;
}

lv_obj_t *ui_utils_create_progress_default(lv_obj_t *parent)
{
    return ui_utils_create_progress(parent, NULL);
}

int32_t ui_utils_get_progress_value(lv_obj_t *progress)
{
    if (!progress) return 0;
    return lv_bar_get_value(progress);
}

void ui_utils_set_progress_value(lv_obj_t *progress, int32_t value)
{
    if (!progress) return;
    lv_bar_set_value(progress, value, LV_ANIM_OFF);
}

lv_obj_t *ui_utils_create_input(lv_obj_t *parent, lv_event_cb_t callback, void *user_data,
                                const ui_input_style_t *style)
{
    lv_obj_t *input = lv_textarea_create(parent);
    
    if (style) {
        lv_obj_set_size(input, style->width, style->height);
        lv_obj_set_style_bg_color(input, style->bg_color, 0);
        lv_obj_set_style_border_color(input, style->border_color, 0);
        lv_obj_set_style_border_width(input, 1, 0);
        lv_obj_set_style_text_color(input, style->text_color, 0);
        if (style->font) {
            lv_obj_set_style_text_font(input, style->font, 0);
        }
        lv_obj_set_style_radius(input, style->radius, 0);
        if (style->placeholder) {
            lv_textarea_set_placeholder_text(input, style->placeholder);
        }
    } else {
        lv_obj_set_size(input, 200, 40);
        lv_obj_set_style_bg_color(input, lv_color_hex(UI_UTILS_COLOR_CARD_BG), 0);
        lv_obj_set_style_border_color(input, lv_color_hex(UI_UTILS_COLOR_BORDER), 0);
        lv_obj_set_style_border_width(input, 1, 0);
        lv_obj_set_style_text_color(input, lv_color_hex(UI_UTILS_COLOR_TEXT_PRIMARY), 0);
        lv_obj_set_style_text_font(input, UI_UTILS_FONT_DEFAULT, 0);
        lv_obj_set_style_radius(input, UI_UTILS_RADIUS_DEFAULT, 0);
    }
    
    lv_textarea_set_one_line(input, true);
    
    if (callback) {
        lv_obj_add_event_cb(input, callback, LV_EVENT_ALL, user_data);
    }
    
    return input;
}

lv_obj_t *ui_utils_create_input_default(lv_obj_t *parent, lv_event_cb_t callback, void *user_data)
{
    return ui_utils_create_input(parent, callback, user_data, NULL);
}

void ui_utils_set_input_text(lv_obj_t *input, const char *text)
{
    if (!input || !text) return;
    lv_textarea_set_text(input, text);
}

const char *ui_utils_get_input_text(lv_obj_t *input)
{
    if (!input) return NULL;
    return lv_textarea_get_text(input);
}