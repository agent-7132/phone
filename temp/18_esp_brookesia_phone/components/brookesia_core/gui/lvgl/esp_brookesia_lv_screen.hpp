/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <cstdlib>
#include "lvgl.h"
#include "style/esp_brookesia_gui_style.hpp"
#include "esp_brookesia_lv_object.hpp"

namespace esp_brookesia::gui {

/**
 * @class LvScreen
 * @brief LVGL屏幕封装类
 * 
 * LvScreen类继承自LvObject，专门用于创建和管理LVGL屏幕对象。
 * 屏幕对象是LVGL中最高层级的容器，用于承载其他UI组件。
 */
class LvScreen: public LvObject {
public:
    using LvObject::LvObject;

    /**
     * @brief 默认构造函数
     * 
     * 创建一个新的LVGL屏幕对象，并初始化默认样式。
     */
    LvScreen();

    /**
     * @brief 加载屏幕
     * 
     * 将此屏幕设置为当前活动屏幕。
     * 
     * @return true 成功，false 失败
     */
    bool load();
};

/**
 * @brief LvScreen唯一指针类型
 */
using LvScreenUniquePtr = std::unique_ptr<LvScreen>;

} // namespace esp_brookesia::gui