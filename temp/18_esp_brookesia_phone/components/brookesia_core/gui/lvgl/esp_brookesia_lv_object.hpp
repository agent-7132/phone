/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <cstdlib>
#include "lvgl.h"
#include "esp_brookesia_lv_helper.hpp"
#include "style/esp_brookesia_gui_style.hpp"

namespace esp_brookesia::gui {

/**
 * @class LvObject
 * @brief LVGL对象封装类
 * 
 * LvObject类是对LVGL原生对象(lv_obj_t)的C++封装，提供了面向对象的接口来管理LVGL对象的生命周期、样式和事件。
 * 支持自动删除模式，当对象被销毁时自动清理LVGL原生对象。
 */
class LvObject {
public:
    /**
     * @brief 构造函数
     * 
     * @param p LVGL原生对象指针
     * @param is_auto_delete 是否自动删除LVGL原生对象（默认为true）
     */
    explicit LvObject(lv_obj_t *p, bool is_auto_delete = true);

    /**
     * @brief 析构函数
     * 
     * 如果启用了自动删除模式且对象有效，则自动删除LVGL原生对象。
     */
    ~LvObject();

    /**
     * @brief 删除拷贝构造函数和赋值运算符，防止对象拷贝
     */
    LvObject(const LvObject &other) = delete;
    LvObject &operator=(const LvObject &other) = delete;

    /**
     * @brief 移动构造函数
     * 
     * 转移LVGL原生对象的所有权。
     */
    LvObject(LvObject &&other):
        _native_handle(other._native_handle)
    {
        other._native_handle = nullptr;
    }

    /**
     * @brief 移动赋值运算符
     * 
     * 转移LVGL原生对象的所有权。
     */
    LvObject &operator=(LvObject &&other)
    {
        if (this != &other) {
            _native_handle = other._native_handle;
            other._native_handle = nullptr;
        }
        return *this;
    }

    /**
     * @brief 设置LVGL样式
     * 
     * 将指定样式应用到对象的主部分和默认状态。
     * 
     * @param style LVGL样式指针
     * @return true 成功，false 失败
     */
    bool setStyle(lv_style_t *style);

    /**
     * @brief 移除LVGL样式
     * 
     * 如果style为nullptr则移除所有样式，否则移除指定样式。
     * 
     * @param style LVGL样式指针，为nullptr时移除所有样式
     * @return true 成功，false 失败
     */
    bool removeStyle(lv_style_t *style);

    /**
     * @brief 设置尺寸样式属性
     * 
     * 设置对象的宽度、高度和圆角。
     * 
     * @param size 尺寸配置
     * @return true 成功，false 失败
     */
    bool setStyleAttribute(const StyleSize &size);

    /**
     * @brief 设置字体样式属性
     * 
     * 设置对象的文本字体。
     * 
     * @param font 字体配置
     * @return true 成功，false 失败
     */
    bool setStyleAttribute(const StyleFont &font);

    /**
     * @brief 设置对齐样式属性
     * 
     * 设置对象相对于父对象的对齐方式。
     * 
     * @param align 对齐配置
     * @return true 成功，false 失败
     */
    bool setStyleAttribute(const StyleAlign &align);

    /**
     * @brief 设置Flex布局样式属性
     * 
     * 设置对象的Flex布局参数。
     * 
     * @param layout Flex布局配置
     * @return true 成功，false 失败
     */
    bool setStyleAttribute(const StyleLayoutFlex &layout);

    /**
     * @brief 设置间距样式属性
     * 
     * 设置对象的内边距和行列间距。
     * 
     * @param gap 间距配置
     * @return true 成功，false 失败
     */
    bool setStyleAttribute(const StyleGap &gap);

    /**
     * @brief 设置颜色样式属性
     * 
     * 设置对象指定类型的颜色和透明度。
     * 
     * @param color_type 颜色类型（背景、文本、边框等）
     * @param color 颜色配置
     * @return true 成功，false 失败
     */
    bool setStyleAttribute(StyleColorItem color_type, const StyleColor &color);

    /**
     * @brief 设置宽度样式属性
     * 
     * 设置对象的边框宽度或轮廓宽度。
     * 
     * @param width_type 宽度类型（边框、轮廓）
     * @param width 宽度值
     * @return true 成功，false 失败
     */
    bool setStyleAttribute(StyleWidthItem width_type, int width);

    /**
     * @brief 设置背景图像样式属性
     * 
     * 设置对象的背景图像及其重着色。
     * 
     * @param image 图像配置
     * @return true 成功，false 失败
     */
    bool setStyleAttribute(const StyleImage &image);

    /**
     * @brief 设置相对于目标对象的对齐样式属性
     * 
     * 设置当前对象相对于另一个对象的对齐方式。
     * 
     * @param target 目标对象
     * @param align 对齐配置
     * @return true 成功，false 失败
     */
    bool setStyleAttribute(LvObject &target, const StyleAlign &align);

    /**
     * @brief 设置样式标志位
     * 
     * 启用或禁用对象的样式标志。
     * 
     * @param flags 样式标志
     * @param enable 是否启用
     * @return true 成功，false 失败
     */
    bool setStyleAttribute(StyleFlag flags, bool enable);

    /**
     * @brief 设置对象X坐标
     * 
     * @param x X坐标值
     * @return true 成功，false 失败
     */
    bool setX(int x);

    /**
     * @brief 设置对象Y坐标
     * 
     * @param y Y坐标值
     * @return true 成功，false 失败
     */
    bool setY(int y);

    /**
     * @brief 滚动到指定Y位置
     * 
     * @param y 目标Y位置
     * @param is_animated 是否启用动画
     * @return true 成功，false 失败
     */
    bool scrollY_To(int y, bool is_animated);

    /**
     * @brief 将对象移到最前面
     * 
     * @return true 成功，false 失败
     */
    bool moveForeground();

    /**
     * @brief 将对象移到最后面
     * 
     * @return true 成功，false 失败
     */
    bool moveBackground();

    /**
     * @brief 添加事件回调
     * 
     * @param cb 回调函数
     * @param code 事件代码
     * @param user_data 用户数据
     * @return true 成功，false 失败
     */
    bool addEventCallback(lv_event_cb_t cb, lv_event_code_t code, void *user_data);

    /**
     * @brief 删除事件回调（带用户数据）
     * 
     * @param cb 回调函数
     * @param code 事件代码
     * @param user_data 用户数据
     * @return true 成功，false 失败
     */
    bool delEventCallback(lv_event_cb_t cb, lv_event_code_t code, void *user_data);

    /**
     * @brief 删除事件回调（仅回调函数）
     * 
     * 删除与指定回调函数关联的所有事件。
     * 
     * @param cb 回调函数
     * @return true 成功，false 失败
     */
    bool delEventCallback(lv_event_cb_t cb);

    /**
     * @brief 检查对象是否有效
     * 
     * @return true 有效，false 无效
     */
    bool isValid() const
    {
        return (_native_handle != nullptr) && lv_obj_is_valid(_native_handle);
    }

    /**
     * @brief 检查对象是否具有指定状态
     * 
     * @param state 状态标志
     * @return true 具有该状态，false 不具有
     */
    bool hasState(lv_state_t state) const;

    /**
     * @brief 检查对象是否具有指定标志
     * 
     * @param flags 样式标志
     * @return true 具有该标志，false 不具有
     */
    bool hasFlags(StyleFlag flags) const;

    /**
     * @brief 获取对象X坐标
     * 
     * @param x X坐标输出参数
     * @return true 成功，false 失败
     */
    bool getX(int &x) const;

    /**
     * @brief 获取对象Y坐标
     * 
     * @param y Y坐标输出参数
     * @return true 成功，false 失败
     */
    bool getY(int &y) const;

    /**
     * @brief 获取对象区域
     * 
     * @param area 对象区域输出参数
     * @return true 成功，false 失败
     */
    bool getArea(lv_area_t &area) const;

    /**
     * @brief 获取LVGL原生对象指针
     * 
     * @return lv_obj_t* LVGL原生对象指针
     */
    lv_obj_t *getNativeHandle() const
    {
        return _native_handle;
    }

private:
    bool _is_auto_delete = true;   /*!< 是否自动删除LVGL原生对象 */
    lv_obj_t *_native_handle = nullptr; /*!< LVGL原生对象指针 */
};

/**
 * @brief LvObject共享指针类型
 */
using LvObjectSharedPtr = std::shared_ptr<LvObject>;

/**
 * @brief LvObject唯一指针类型
 */
using LvObjectUniquePtr = std::unique_ptr<LvObject>;

} // namespace esp_brookesia::gui