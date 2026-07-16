#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *keyboard;
    lv_obj_t *ime;
    lv_obj_t *current_textarea;
    bool visible;
    bool use_pinyin;
} input_tool_t;

input_tool_t *input_tool_create(lv_obj_t *parent);
void input_tool_destroy(input_tool_t *tool);
void input_tool_attach_textarea(input_tool_t *tool, lv_obj_t *textarea);
void input_tool_show(input_tool_t *tool);
void input_tool_hide(input_tool_t *tool);
void input_tool_set_mode(input_tool_t *tool, lv_keyboard_mode_t mode);
void input_tool_set_textarea(input_tool_t *tool, lv_obj_t *textarea);
void input_tool_enable_pinyin(input_tool_t *tool, bool enable);
bool input_tool_is_pinyin_enabled(input_tool_t *tool);

#ifdef __cplusplus
}
#endif
