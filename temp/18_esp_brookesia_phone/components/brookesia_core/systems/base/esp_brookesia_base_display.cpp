/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_brookesia_systems_internal.h"
#include "lvgl/esp_brookesia_lv_container.hpp"
#include "lvgl/esp_brookesia_lv_helper.hpp"
#include "lvgl/esp_brookesia_lv_screen.hpp"
#include <src/core/lv_obj_style_gen.h>
#include <src/display/lv_display.h>
#if !ESP_BROOKESIA_BASE_DISPLAY_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_base_utils.hpp"
#include "esp_brookesia_base_display.hpp"
#include "esp_brookesia_base_app.hpp"
#include "esp_brookesia_base_context.hpp"

using namespace std;
using namespace esp_brookesia::gui;

namespace esp_brookesia::systems::base {

/**
 * @brief Display类构造函数
 * 
 * 初始化Display对象，保存系统上下文和配置数据引用。
 * 
 * @param core 系统上下文引用
 * @param data Display配置数据引用
 */
Display::Display(Context &core, const Data &data)
    : _system_context(core)
    , _core_data(data)
{
}

/**
 * @brief Display类析构函数
 * 
 * 调用del()方法清理资源。
 */
Display::~Display()
{
    ESP_UTILS_LOGD("Destroy(@0x%p)", this);
    if (!del()) {
        ESP_UTILS_LOGE("Delete failed");
    }
}

/**
 * @brief 显示容器边框（调试用）
 * 
 * 将所有容器样式的边框宽度设置为配置值，用于调试布局。
 * 
 * @return true 成功，false 失败
 */
bool Display::showContainerBorder(void)
{
    ESP_UTILS_LOGD("Show container border");
    ESP_UTILS_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Not initialized");

    for (size_t i = 0; i < _container_styles.size(); i++) {
        lv_style_set_outline_width(&_container_styles[i], _core_data.container.styles[i].outline_width);
    }

    return true;
}

/**
 * @brief 隐藏容器边框
 * 
 * 将所有容器样式的边框宽度设置为0。
 * 
 * @return true 成功，false 失败
 */
bool Display::hideContainerBorder(void)
{
    ESP_UTILS_LOGD("Hide container border");
    ESP_UTILS_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Not initialized");

    for (auto &style : _container_styles) {
        lv_style_set_outline_width(&style, 0);
    }

    return true;
}

/**
 * @brief 获取核心容器样式（循环获取）
 * 
 * 循环返回容器样式数组中的样式，用于为不同容器分配不同的调试样式。
 * 
 * @return lv_style_t* 容器样式指针
 */
lv_style_t *Display::getCoreContainerStyle(void)
{
    int index = _container_style_index++;
    if (_container_style_index > (_container_styles.size() - 1)) {
        _container_style_index = 0;
    }

    return &_container_styles[index];
}

/**
 * @brief 校准核心对象尺寸
 * 
 * 调用StyleSize的calibrate方法和内部校准函数完成尺寸校准。
 * 
 * @param parent 父对象尺寸
 * @param target 目标对象尺寸（将被校准）
 * @return true 成功，false 失败
 */
bool Display::calibrateCoreObjectSize(const StyleSize &parent, StyleSize &target) const
{
    ESP_UTILS_CHECK_FALSE_RETURN(target.calibrate(parent), false, "Calibrate failed");
    ESP_UTILS_CHECK_FALSE_RETURN(calibrateStyleSizeInternal(target), false, "Calibrate internal failed");

    return true;
}

/**
 * @brief 校准核心对象尺寸（带宽度/高度检查控制）
 * 
 * 调用StyleSize的calibrate方法（带检查参数）和内部校准函数完成尺寸校准。
 * 
 * @param parent 父对象尺寸
 * @param target 目标对象尺寸（将被校准）
 * @param check_width 是否检查宽度
 * @param check_height 是否检查高度
 * @return true 成功，false 失败
 */
bool Display::calibrateCoreObjectSize(
    const StyleSize &parent, StyleSize &target, bool check_width, bool check_height
) const
{
    ESP_UTILS_CHECK_FALSE_RETURN(target.calibrate(parent, check_width, check_height), false, "Calibrate failed");
    ESP_UTILS_CHECK_FALSE_RETURN(calibrateStyleSizeInternal(target), false, "Calibrate internal failed");

    return true;
}

/**
 * @brief 校准核心对象尺寸（带零值允许控制）
 * 
 * 调用StyleSize的calibrate方法（带零值允许参数）和内部校准函数完成尺寸校准。
 * 
 * @param parent 父对象尺寸
 * @param target 目标对象尺寸（将被校准）
 * @param allow_zero 是否允许零值
 * @return true 成功，false 失败
 */
bool Display::calibrateCoreObjectSize(
    const StyleSize &parent, StyleSize &target, bool allow_zero
) const
{
    ESP_UTILS_CHECK_FALSE_RETURN(target.calibrate(parent, allow_zero), false, "Calibrate failed");
    ESP_UTILS_CHECK_FALSE_RETURN(calibrateStyleSizeInternal(target), false, "Calibrate internal failed");

    return true;
}

/**
 * @brief 校准核心字体
 * 
 * 通过lambda函数将字体查找逻辑注入StyleFont的calibrate方法中，完成字体校准。
 * 
 * @param parent 父对象尺寸（可为nullptr）
 * @param target 目标字体（将被校准）
 * @return true 成功，false 失败
 */
bool Display::calibrateCoreFont(const StyleSize *parent, StyleFont &target) const
{
    ESP_UTILS_CHECK_FALSE_RETURN(target.calibrate(
                                     parent,
    [&](int size_px) {
        return (const void *)getFontBySize(size_px);
    },
    [&](int height, int *size_px) {
        return (const void *)getFontByHeight(height, size_px);
    },
    [&](const void *font) {
        return ((const lv_font_t *)font)->line_height;
    }
                                 ), false, "Calibrate failed");

    return true;
}

/**
 * @brief 校准核心图标图像
 * 
 * 调用StyleImage的calibrate方法完成图标校准。
 * 
 * @param target 目标图标图像
 * @return true 成功，false 失败
 */
bool Display::calibrateCoreIconImage(const StyleImage &target) const
{
    ESP_UTILS_CHECK_FALSE_RETURN(target.calibrate(), false, "Calibrate failed");

    return true;
}

/**
 * @brief 处理主屏幕加载
 * 
 * 将主屏幕加载为当前活动屏幕。
 * 
 * @return true 成功，false 失败
 */
bool Display::processMainScreenLoad(void)
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_FALSE_RETURN(_main_screen->isValid(), false, "Invalid main screen");

    lv_scr_load(_main_screen->getNativeHandle());

    return true;
}

/**
 * @brief 初始化Display
 * 
 * 执行以下初始化步骤：
 * 1. 保存当前LVGL屏幕状态
 * 2. 创建主屏幕和系统屏幕对象
 * 3. 初始化容器样式
 * 4. 设置屏幕对象属性
 * 5. 更新显示样式
 * 6. 设置系统层和加载主屏幕
 * 
 * @return true 成功，false 失败
 */
bool Display::begin(void)
{
    lv_display_t *display = _system_context.getDisplayDevice();

    ESP_UTILS_LOGD("Begin(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(!checkCoreInitialized(), false, "Already initialized");
    ESP_UTILS_CHECK_NULL_RETURN(display, false, "Invalid display device");

    saveLvScreens();

    /* Create objects */
    // Main screen
    _main_screen = std::make_unique<LvScreen>();
    ESP_UTILS_CHECK_FALSE_RETURN(_main_screen->isValid(), false, "Invalid lvgl current screen");
    _main_screen_obj = std::make_unique<LvContainer>(_main_screen.get());
    ESP_UTILS_CHECK_FALSE_RETURN(_main_screen_obj->isValid(), false, "Create main screen failed");
    // System screen
    _system_screen = std::make_unique<LvScreen>();
    ESP_UTILS_CHECK_FALSE_RETURN(_system_screen->isValid(), false, "Invalid lvgl top screen");
    _system_screen_obj = std::make_unique<LvContainer>(_system_screen.get());
    ESP_UTILS_CHECK_FALSE_RETURN(_system_screen_obj->isValid(), false, "Create system screen failed");

    /* Setup objects */
    // Container styles
    for (auto &style : _container_styles) {
        lv_style_init(&style);
        lv_style_set_size(&style, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_style_set_radius(&style, 0);
        lv_style_set_border_width(&style, 0);
        lv_style_set_pad_all(&style, 0);
        lv_style_set_pad_gap(&style, 0);
        lv_style_set_bg_opa(&style, LV_OPA_TRANSP);
        lv_style_set_outline_width(&style, 0);
    }
    // Main screen
    lv_obj_align(_main_screen_obj->getNativeHandle(), LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(_main_screen_obj->getNativeHandle(), LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(_main_screen_obj->getNativeHandle(), getCoreContainerStyle(), 0);
    // System screen
    lv_obj_align(_system_screen_obj->getNativeHandle(), LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(_system_screen_obj->getNativeHandle(), LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(_system_screen_obj->getNativeHandle(), getCoreContainerStyle(), 0);

    // Update object style
    ESP_UTILS_CHECK_FALSE_GOTO(updateByNewData(), err, "Update object style failed");
    ESP_UTILS_CHECK_FALSE_GOTO(hideContainerBorder(), err, "Hide container border failed");

    display->sys_layer = _system_screen->getNativeHandle();
    lv_screen_load(_main_screen->getNativeHandle());

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(del(), false, "Delete core display failed");

    return false;
}

/**
 * @brief 销毁Display
 * 
 * 执行以下清理步骤：
 * 1. 恢复LVGL屏幕状态
 * 2. 重置所有容器样式
 * 3. 释放屏幕对象智能指针
 * 4. 清空字体映射
 * 
 * @return true 成功，false 失败
 */
bool Display::del(void)
{
    ESP_UTILS_LOGD("Delete(0x%p)", this);

    if (!checkCoreInitialized()) {
        return true;
    }

    loadLvScreens();

    for (auto &style : _container_styles) {
        lv_style_reset(&style);
    }
    _main_screen_obj = nullptr;
    _system_screen_obj = nullptr;
    _main_screen = nullptr;
    _system_screen = nullptr;
    _container_style_index = 0;
    _default_size_font_map.clear();
    _default_height_font_map.clear();
    _update_size_font_map.clear();
    _update_height_font_map.clear();

    return true;
}

/**
 * @brief 根据新数据更新显示
 * 
 * 更新主屏幕和系统屏幕的样式属性，包括尺寸、裁剪、背景颜色、壁纸等。
 * 同时更新字体映射和容器边框样式。
 * 
 * @return true 成功，false 失败
 */
bool Display::updateByNewData(void)
{
    const StyleSize &screen_size = _system_context.getData().screen_size;

    ESP_UTILS_LOGD("Update core display by new data");

    ESP_UTILS_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Not initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(
        _main_screen_obj->setStyleAttribute(screen_size), false, "Set main screen size failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        _main_screen_obj->setStyleAttribute(STYLE_FLAG_CLIP_CORNER, true), false, "Set system screen clip corner failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        _main_screen_obj->setStyleAttribute(STYLE_COLOR_ITEM_BACKGROUND, _core_data.background.color),
        false, "Set main screen background color failed"
    );
    if (_core_data.background.wallpaper_image_resource.resource != nullptr) {
        ESP_UTILS_CHECK_FALSE_RETURN(_main_screen_obj->setStyleAttribute(
                                         _core_data.background.wallpaper_image_resource), false, "Set main screen wallpaper image failed"
                                    );
    }

    ESP_UTILS_CHECK_FALSE_RETURN(
        _system_screen_obj->setStyleAttribute(screen_size), false, "Set system screen size failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        _system_screen_obj->setStyleAttribute(STYLE_FLAG_CLIP_CORNER, true), false, "Set system screen clip corner failed"
    );

    // Text
    _default_size_font_map = _update_size_font_map;

    // Container styles
    for (size_t i = 0; i < _container_styles.size(); i++) {
        lv_style_set_outline_width(&_container_styles[i], _core_data.container.styles[i].outline_width);
        lv_style_set_outline_color(&_container_styles[i],
                                   lv_color_hex(_core_data.container.styles[i].outline_color.color));
        lv_style_set_outline_opa(&_container_styles[i], _core_data.container.styles[i].outline_color.opacity);
    }

    return true;
}

/**
 * @brief 校准核心数据
 * 
 * 处理字体配置：
 * 1. 清空现有的字体映射
 * 2. 注册用户配置的字体
 * 3. 检查是否有缺失的字体大小，若有则尝试使用内部字体补全
 * 
 * @param data 待校准的数据
 * @return true 成功，false 失败
 */
bool Display::calibrateCoreData(Data &data)
{
    const lv_font_t *font_resource = nullptr;

    // Text
    _update_size_font_map.clear();
    _update_height_font_map.clear();
    for (int i = 0; i < data.text.default_fonts_num; i++) {
        ESP_UTILS_CHECK_VALUE_RETURN(data.text.default_fonts[i].size_px, StyleFont::FONT_SIZE_MIN,
                                     StyleFont::FONT_SIZE_MAX, false, "Invalid default font(%d) size", i);
        ESP_UTILS_CHECK_NULL_RETURN(data.text.default_fonts[i].font_resource, false, "Invalid default font(%d) dsc", i);
        font_resource = (lv_font_t *)data.text.default_fonts[i].font_resource;
        // Save font for function ``
        _update_size_font_map[data.text.default_fonts[i].size_px] = font_resource;
        _update_height_font_map[font_resource->line_height] = font_resource;
    }
    // Check if all default fonts are set, if not, use internal fonts
    for (int i = StyleFont::FONT_SIZE_MIN; i <= StyleFont::FONT_SIZE_MAX; i += 2) {
        if (_update_size_font_map.find(i) == _update_size_font_map.end()) {
            ESP_UTILS_LOGW("Default font size(%d) is not found, try to use internal font instead", i);
            if (!esp_brookesia_core_utils_get_internal_font_by_size(i, &font_resource)) {
                continue;
            }
            _update_size_font_map[i] = font_resource;
            if (_update_height_font_map.find(font_resource->line_height) == _update_height_font_map.end()) {
                _update_height_font_map[font_resource->line_height] = font_resource;
            }
        }
    }

    return true;
}

/**
 * @brief 保存当前LVGL屏幕状态
 * 
 * 保存当前活动屏幕和系统层屏幕，以便在Display销毁时恢复。
 */
void Display::saveLvScreens(void)
{
    auto display = _system_context.getDisplayDevice();
    _lv_main_screen = lv_display_get_screen_active(display);
    _lv_system_screen = lv_display_get_layer_sys(display);
}

/**
 * @brief 恢复LVGL屏幕状态
 * 
 * 将系统层和活动屏幕恢复到保存前的状态。
 */
void Display::loadLvScreens(void)
{
    auto display = _system_context.getDisplayDevice();
    display->sys_layer = _lv_system_screen;
    lv_scr_load(_lv_main_screen);
}

/**
 * @brief 内部尺寸校准函数
 * 
 * 将StyleSize中的特殊常量值转换为LVGL对应的常量值：
 * - LENGTH_AUTO -> LV_SIZE_CONTENT
 * - RADIUS_CIRCLE -> LV_RADIUS_CIRCLE
 * 
 * @param target 目标尺寸
 * @return true 成功，false 失败
 */
bool Display::calibrateStyleSizeInternal(StyleSize &target) const
{
    if (target.width == StyleSize::LENGTH_AUTO) {
        target.width = LV_SIZE_CONTENT;
    }
    if (target.height == StyleSize::LENGTH_AUTO) {
        target.height = LV_SIZE_CONTENT;
    }
    if (target.radius == StyleSize::RADIUS_CIRCLE) {
        target.radius = LV_RADIUS_CIRCLE;
    }

    return true;
}

/**
 * @brief 根据字号获取字体
 * 
 * 在字体映射中查找指定字号的字体。
 * 
 * @param size_px 字体大小（像素）
 * @return const lv_font_t* 字体指针，失败返回nullptr
 */
const lv_font_t *Display::getFontBySize(int size) const
{
    ESP_UTILS_CHECK_VALUE_RETURN(
        size, StyleFont::FONT_SIZE_MIN, StyleFont::FONT_SIZE_MAX, nullptr, "Invalid size"
    );

    auto it = _update_size_font_map.find(size);
    ESP_UTILS_CHECK_FALSE_RETURN(it != _update_size_font_map.end(), nullptr, "Font size(%d) is not found", size);

    return it->second;
}

/**
 * @brief 根据高度获取字体
 * 
 * 在字体映射中查找不小于指定高度的字体，优先选择最接近的字体。
 * 
 * @param height 字体高度
 * @param size_px 输出字体大小（像素）
 * @return const lv_font_t* 字体指针，失败返回nullptr
 */
const lv_font_t *Display::getFontByHeight(int height, int *size_px) const
{
    int ret_size = 0;

    auto lower = _update_height_font_map.lower_bound(height);
    if ((lower->first != height) && (lower != _update_height_font_map.begin())) {
        lower--;
    }
    ESP_UTILS_CHECK_FALSE_RETURN(lower != _update_height_font_map.end(), nullptr, "Font height(%d) is not found", height);

    if (size_px != nullptr) {
        for (auto &it : _update_size_font_map) {
            if (it.second == lower->second) {
                ret_size = it.first;
                break;
            }
        }
        ESP_UTILS_CHECK_FALSE_RETURN(ret_size != 0, nullptr, "Font size is not found");
        *size_px = ret_size;
    }

    return lower->second;
}

} // namespace esp_brookesia::systems::base