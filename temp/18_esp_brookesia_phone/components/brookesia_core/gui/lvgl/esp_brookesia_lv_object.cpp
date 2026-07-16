/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string>
#include <cmath>
#include "esp_brookesia_gui_internal.h"
#if !ESP_BROOKESIA_LVGL_OBJECT_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_lv_utils.hpp"
#include "esp_brookesia_lv_object.hpp"

namespace esp_brookesia::gui {

/**
 * @brief LvObject类构造函数
 * 
 * 初始化LVGL对象封装类，注册LV_EVENT_DELETE事件回调以在LVGL对象被删除时更新内部指针。
 * 
 * @param p LVGL原生对象指针
 * @param is_auto_delete 是否自动删除LVGL原生对象
 */
LvObject::LvObject(lv_obj_t *p, bool is_auto_delete):
    _is_auto_delete(is_auto_delete),
    _native_handle(p)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_EXIT((p != nullptr) && checkLvObjIsValid(p), "Invalid object pointer(@0x%p)", p);

    lv_obj_add_event_cb(p, [](lv_event_t *e) {
        ESP_UTILS_LOG_TRACE_ENTER();

        auto *obj = static_cast<LvObject *>(lv_event_get_user_data(e));
        ESP_UTILS_CHECK_NULL_EXIT(obj, "Invalid user data");

        obj->_native_handle = nullptr;

        ESP_UTILS_LOG_TRACE_EXIT();
    }, LV_EVENT_DELETE, this);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
}

/**
 * @brief LvObject类析构函数
 * 
 * 如果启用了自动删除模式且对象有效，则自动删除LVGL原生对象。
 */
LvObject::~LvObject()
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    if (_is_auto_delete && isValid()) {
        lv_obj_delete(_native_handle);
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
}

/**
 * @brief 设置LVGL样式
 * 
 * 将指定样式应用到对象的主部分(LV_PART_MAIN)和默认状态(LV_STATE_DEFAULT)。
 * 
 * @param style LVGL样式指针
 * @return true 成功，false 失败
 */
bool LvObject::setStyle(lv_style_t *style)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: style(0x%p)", style);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");
    ESP_UTILS_CHECK_NULL_RETURN(style, false, "Invalid style");

    lv_obj_add_style(_native_handle, style, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 移除LVGL样式
 * 
 * 如果style为nullptr则移除所有样式，否则移除指定样式。
 * 
 * @param style LVGL样式指针，为nullptr时移除所有样式
 * @return true 成功，false 失败
 */
bool LvObject::removeStyle(lv_style_t *style)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: style(0x%p)", style);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    if (style == nullptr) {
        lv_obj_remove_style_all(_native_handle);
    } else {
        lv_obj_remove_style(_native_handle, style, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 设置宽度样式属性
 * 
 * 根据宽度类型设置边框宽度或轮廓宽度。
 * 
 * @param width_type 宽度类型（边框、轮廓）
 * @param width 宽度值
 * @return true 成功，false 失败
 */
bool LvObject::setStyleAttribute(StyleWidthItem width_type, int width)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: width_type(%d), width(%d)", width_type, width);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    switch (width_type) {
    case STYLE_WIDTH_ITEM_BORDER:
        lv_obj_set_style_border_width(_native_handle, width, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
        break;
    case STYLE_WIDTH_ITEM_OUTLINE:
        lv_obj_set_style_outline_width(_native_handle, width, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
        break;
    default:
        break;
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 设置尺寸样式属性
 * 
 * 设置对象的宽度、高度和圆角。
 * 
 * @param size 尺寸配置
 * @return true 成功，false 失败
 */
bool LvObject::setStyleAttribute(const gui::StyleSize &size)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: size(width=%d, height=%d, radius=%d)", size.width, size.height, size.radius);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_set_style_width(_native_handle, size.width, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
    lv_obj_set_style_height(_native_handle, size.height, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
    lv_obj_set_style_radius(_native_handle, size.radius, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 设置字体样式属性
 * 
 * 设置对象的文本字体。
 * 
 * @param font 字体配置
 * @return true 成功，false 失败
 */
bool LvObject::setStyleAttribute(const gui::StyleFont &font)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: font(font_resource=0x%p)", font.font_resource);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_set_style_text_font(_native_handle, (lv_font_t *)font.font_resource, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 设置对齐样式属性
 * 
 * 设置对象相对于父对象的对齐方式。
 * 
 * @param align 对齐配置
 * @return true 成功，false 失败
 */
bool LvObject::setStyleAttribute(const gui::StyleAlign &align)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: align(type=%d, offset_x=%d, offset_y=%d)", align.type, align.offset_x, align.offset_y);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_align(_native_handle, toLvAlign(align.type), align.offset_x, align.offset_y);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 设置Flex布局样式属性
 * 
 * 设置对象的Flex布局参数，包括布局方式、主轴对齐、交叉轴对齐和轨道对齐。
 * 
 * @param layout Flex布局配置
 * @return true 成功，false 失败
 */
bool LvObject::setStyleAttribute(const gui::StyleLayoutFlex &layout)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: layout(flow=%d, main_place=%d, cross_place=%d, track_place=%d)",
                   layout.flow, layout.main_place, layout.cross_place, layout.track_place);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_set_style_layout(_native_handle, LV_LAYOUT_FLEX, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
    lv_obj_set_style_flex_flow(_native_handle, toLvFlexFlow(layout.flow), (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
    lv_obj_set_style_flex_main_place(
        _native_handle, toLvFlexAlign(layout.main_place), (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT
    );
    lv_obj_set_style_flex_cross_place(
        _native_handle, toLvFlexAlign(layout.cross_place), (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT
    );
    lv_obj_set_style_flex_track_place(
        _native_handle, toLvFlexAlign(layout.track_place), (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT
    );

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 设置间距样式属性
 * 
 * 设置对象的内边距(left, right, top, bottom)和行列间距(row, column)。
 * 
 * @param gap 间距配置
 * @return true 成功，false 失败
 */
bool LvObject::setStyleAttribute(const gui::StyleGap &gap)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: gap(left=%d, right=%d, top=%d, bottom=%d, row=%d, column=%d)",
                   gap.left, gap.right, gap.top, gap.bottom, gap.row, gap.column);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_set_style_pad_left(_native_handle, gap.left, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(_native_handle, gap.right, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(_native_handle, gap.top, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(_native_handle, gap.bottom, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(_native_handle, gap.row, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(_native_handle, gap.column, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 设置颜色样式属性
 * 
 * 根据颜色类型设置背景色、文本色或边框色及其透明度。
 * 
 * @param item 颜色类型
 * @param color 颜色配置
 * @return true 成功，false 失败
 */
bool LvObject::setStyleAttribute(
    gui::StyleColorItem item, const gui::StyleColor &color
)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: item(%d), color(color=0x%x, opacity=%d)", item, color.color, color.opacity);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    switch (item) {
    case STYLE_COLOR_ITEM_BACKGROUND:
        lv_obj_set_style_bg_color(_native_handle, toLvColor(color.color), (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(_native_handle, color.opacity, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
        break;
    case STYLE_COLOR_ITEM_TEXT:
        lv_obj_set_style_text_color(_native_handle, toLvColor(color.color), (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(_native_handle, color.opacity, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
        break;
    case STYLE_COLOR_ITEM_BORDER:
        lv_obj_set_style_border_color(_native_handle, toLvColor(color.color), (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(_native_handle, color.opacity, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
        break;
    default:
        break;
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 设置背景图像样式属性
 * 
 * 设置对象的背景图像资源、重着色颜色和重着色透明度。
 * 
 * @param image 图像配置
 * @return true 成功，false 失败
 */
bool LvObject::setStyleAttribute(const gui::StyleImage &image)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: image(resource=0x%p, recolor.color=0x%x, recolor.opacity=%d)",
                   image.resource, image.recolor.color, image.recolor.opacity);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_set_style_bg_img_src(_native_handle, image.resource, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_recolor(_native_handle, lv_color_hex(image.recolor.color), (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_recolor_opa(_native_handle, image.recolor.opacity, (int)LV_PART_MAIN | (int)LV_STATE_DEFAULT);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 设置相对于目标对象的对齐样式属性
 * 
 * 设置当前对象相对于另一个对象的对齐方式，并更新布局。
 * 
 * @param target 目标对象
 * @param align 对齐配置
 * @return true 成功，false 失败
 */
bool LvObject::setStyleAttribute(LvObject &target, const gui::StyleAlign &align)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD(
        "Param: target(0x%p), align(type=%d, offset_x=%d, offset_y=%d)",
        &target, align.type, align.offset_x, align.offset_y
    );

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");
    ESP_UTILS_CHECK_FALSE_RETURN(target.isValid(), false, "Invalid target");

    lv_obj_align_to(
        _native_handle, target._native_handle, toLvAlign(align.type), align.offset_x, align.offset_y
    );
    lv_obj_update_layout(_native_handle);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 设置样式标志位
 * 
 * 启用或禁用对象的样式标志。对于STYLE_FLAG_CLIP_CORNER标志，还需要同步设置clip_corner属性。
 * 
 * @param flags 样式标志
 * @param enable 是否启用
 * @return true 成功，false 失败
 */
bool LvObject::setStyleAttribute(StyleFlag flags, bool enable)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: flags(%d), enable(%d)", flags, enable);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_flag_t lv_flag = toLvFlags(flags);
    if (lv_flag) {
        if (enable) {
            lv_obj_add_flag(_native_handle, lv_flag);
        } else {
            lv_obj_remove_flag(_native_handle, lv_flag);
        }
    }

    if (flags | STYLE_FLAG_CLIP_CORNER) {
        lv_obj_set_style_clip_corner(
            _native_handle, enable, static_cast<int>(LV_PART_MAIN) | static_cast<int>(LV_STATE_DEFAULT)
        );
        if (enable) {
            lv_obj_remove_flag(_native_handle, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        } else {
            lv_obj_add_flag(_native_handle, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        }
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 设置对象X坐标
 * 
 * @param x X坐标值
 * @return true 成功，false 失败
 */
bool LvObject::setX(int x)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: x(%d)", x);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_set_x(_native_handle, x);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 设置对象Y坐标
 * 
 * @param y Y坐标值
 * @return true 成功，false 失败
 */
bool LvObject::setY(int y)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: y(%d)", y);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_set_y(_native_handle, y);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 滚动到指定Y位置
 * 
 * @param y 目标Y位置
 * @param is_animated 是否启用动画
 * @return true 成功，false 失败
 */
bool LvObject::scrollY_To(int y, bool is_animated)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: y(%d), is_animated(%d)", y, is_animated);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_scroll_to_y(_native_handle, y, is_animated ? LV_ANIM_ON : LV_ANIM_OFF);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 将对象移到最前面
 * 
 * 将对象移到父对象子列表的最前端。
 * 
 * @return true 成功，false 失败
 */
bool LvObject::moveForeground(void)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_move_foreground(_native_handle);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 将对象移到最后面
 * 
 * 将对象移到父对象子列表的最后端。
 * 
 * @return true 成功，false 失败
 */
bool LvObject::moveBackground(void)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_move_background(_native_handle);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 添加事件回调
 * 
 * 为对象注册事件回调函数。
 * 
 * @param cb 回调函数
 * @param code 事件代码
 * @param user_data 用户数据
 * @return true 成功，false 失败
 */
bool LvObject::addEventCallback(lv_event_cb_t cb, lv_event_code_t code, void *user_data)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: cb(0x%p), code(%d), user_data(0x%p)", cb, code, user_data);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_add_event_cb(_native_handle, cb, code, user_data);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 删除事件回调（带用户数据）
 * 
 * 删除与指定回调函数和用户数据关联的事件。
 * 
 * @param cb 回调函数
 * @param code 事件代码
 * @param user_data 用户数据
 * @return true 成功，false 失败
 */
bool LvObject::delEventCallback(lv_event_cb_t cb, lv_event_code_t code, void *user_data)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: cb(0x%p), code(%d), user_data(0x%p)", cb, code, user_data);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_remove_event_cb_with_user_data(_native_handle, cb, user_data);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 删除事件回调（仅回调函数）
 * 
 * 删除与指定回调函数关联的所有事件。
 * 
 * @param cb 回调函数
 * @return true 成功，false 失败
 */
bool LvObject::delEventCallback(lv_event_cb_t cb)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: cb(0x%p)", cb);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_remove_event_cb(_native_handle, cb);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 检查对象是否具有指定状态
 * 
 * @param state 状态标志
 * @return true 具有该状态，false 不具有
 */
bool LvObject::hasState(lv_state_t state) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: state(%d)", state);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    bool result = lv_obj_has_state(_native_handle, state);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return result;
}

/**
 * @brief 检查对象是否具有指定标志
 * 
 * @param flags 样式标志
 * @return true 具有该标志，false 不具有
 */
bool LvObject::hasFlags(StyleFlag flags) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: flags(%d)", flags);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    bool result = true;
    lv_obj_flag_t lv_flag = toLvFlags(flags);
    if (lv_flag) {
        result = lv_obj_has_flag(_native_handle, lv_flag);
    }

    if (flags | STYLE_FLAG_CLIP_CORNER) {
        result &= lv_obj_get_style_clip_corner(_native_handle, LV_PART_MAIN);
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return result;
}

/**
 * @brief 获取对象X坐标
 * 
 * 更新布局后获取对象的X坐标。
 * 
 * @param x X坐标输出参数
 * @return true 成功，false 失败
 */
bool LvObject::getX(int &x) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: x(0x%p)", &x);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_update_layout(_native_handle);
    x = lv_obj_get_x(_native_handle);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 获取对象Y坐标
 * 
 * 更新布局后获取对象的Y坐标。
 * 
 * @param y Y坐标输出参数
 * @return true 成功，false 失败
 */
bool LvObject::getY(int &y) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: y(0x%p)", &y);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_update_layout(_native_handle);
    y = lv_obj_get_y(_native_handle);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

/**
 * @brief 获取对象区域
 * 
 * 更新布局后获取对象的完整区域信息。
 * 
 * @param area 对象区域输出参数
 * @return true 成功，false 失败
 */
bool LvObject::getArea(lv_area_t &area) const
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: area(0x%p)", &area);

    ESP_UTILS_CHECK_FALSE_RETURN(isValid(), false, "Invalid object");

    lv_obj_update_layout(_native_handle);
    lv_obj_get_coords(_native_handle, &area);

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

} // namespace esp_brookesia::gui