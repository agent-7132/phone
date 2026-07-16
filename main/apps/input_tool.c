#include "input_tool.h"
#include <stdlib.h>
#include <string.h>

static void keyboard_event_cb(lv_event_t *e);
static void textarea_focus_cb(lv_event_t *e);

input_tool_t *input_tool_create(lv_obj_t *parent)
{
    input_tool_t *tool = (input_tool_t *)malloc(sizeof(input_tool_t));
    if (!tool) return NULL;
    
    memset(tool, 0, sizeof(input_tool_t));
    
    tool->keyboard = lv_keyboard_create(parent);
    lv_obj_set_size(tool->keyboard, lv_obj_get_width(parent), 200);
    lv_obj_align(tool->keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(tool->keyboard, LV_OBJ_FLAG_HIDDEN);
    
#if LV_USE_IME_PINYIN
    tool->ime = lv_ime_pinyin_create(parent);
    lv_ime_pinyin_set_keyboard(tool->ime, tool->keyboard);
    lv_obj_add_flag(tool->ime, LV_OBJ_FLAG_HIDDEN);
#endif
    
    lv_obj_add_event_cb(tool->keyboard, keyboard_event_cb, LV_EVENT_ALL, tool);
    
    tool->visible = false;
    tool->use_pinyin = false;
    return tool;
}

void input_tool_destroy(input_tool_t *tool)
{
    if (tool) {
        if (tool->keyboard) {
            lv_obj_del(tool->keyboard);
        }
#if LV_USE_IME_PINYIN
        if (tool->ime) {
            lv_obj_del(tool->ime);
        }
#endif
        free(tool);
    }
}

void input_tool_attach_textarea(input_tool_t *tool, lv_obj_t *textarea)
{
    if (!tool || !textarea) return;
    
    lv_obj_add_event_cb(textarea, textarea_focus_cb, LV_EVENT_ALL, tool);
}

void input_tool_show(input_tool_t *tool)
{
    if (!tool || !tool->keyboard) return;
    
    lv_obj_remove_flag(tool->keyboard, LV_OBJ_FLAG_HIDDEN);
#if LV_USE_IME_PINYIN
    if (tool->use_pinyin && tool->ime) {
        lv_obj_remove_flag(tool->ime, LV_OBJ_FLAG_HIDDEN);
    }
#endif
    tool->visible = true;
}

void input_tool_hide(input_tool_t *tool)
{
    if (!tool || !tool->keyboard) return;
    
    lv_obj_add_flag(tool->keyboard, LV_OBJ_FLAG_HIDDEN);
#if LV_USE_IME_PINYIN
    if (tool->ime) {
        lv_obj_add_flag(tool->ime, LV_OBJ_FLAG_HIDDEN);
    }
#endif
    tool->visible = false;
}

void input_tool_set_mode(input_tool_t *tool, lv_keyboard_mode_t mode)
{
    if (!tool || !tool->keyboard) return;
    
    lv_keyboard_set_mode(tool->keyboard, mode);
}

void input_tool_set_textarea(input_tool_t *tool, lv_obj_t *textarea)
{
    if (!tool || !tool->keyboard) return;
    
    tool->current_textarea = textarea;
    lv_keyboard_set_textarea(tool->keyboard, textarea);
}

static void keyboard_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    input_tool_t *tool = (input_tool_t *)lv_event_get_user_data(e);
    
    if (code == LV_EVENT_READY) {
        input_tool_hide(tool);
        if (tool->current_textarea) {
            lv_obj_clear_state(tool->current_textarea, LV_STATE_FOCUSED);
        }
    }
}

static void textarea_focus_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target_obj(e);
    input_tool_t *tool = (input_tool_t *)lv_event_get_user_data(e);
    
    if (code == LV_EVENT_FOCUSED) {
        input_tool_set_textarea(tool, ta);
        input_tool_show(tool);
    } else if (code == LV_EVENT_DEFOCUSED) {
        input_tool_hide(tool);
    }
}

void input_tool_enable_pinyin(input_tool_t *tool, bool enable)
{
    if (!tool) return;
    
    tool->use_pinyin = enable;
    
#if LV_USE_IME_PINYIN
    if (tool->ime) {
        if (enable) {
            lv_ime_pinyin_set_keyboard(tool->ime, tool->keyboard);
        }
    }
#endif
}

bool input_tool_is_pinyin_enabled(input_tool_t *tool)
{
    if (!tool) return false;
    return tool->use_pinyin;
}
