/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
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
 * @class LvContainer
 * @brief LVGL容器封装类
 * 
 * LvContainer类继承自LvObject，专门用于创建和管理LVGL容器对象。
 * 容器对象是通用的布局容器，用于组织和管理其他UI组件。
 */
class LvContainer: public LvObject {
public:
    /**
     * @brief 构造函数
     * 
     * 创建一个新的LVGL容器对象作为指定父对象的子对象。
     * 
     * @param parent 父对象指针，为nullptr时创建失败
     */
    LvContainer(const LvObject *parent);
};

/**
 * @brief LvContainer唯一指针类型
 */
using LvContainerUniquePtr = std::unique_ptr<LvContainer>;

/**
 * @brief LvContainer共享指针类型
 */
using LvContainerSharedPtr = std::shared_ptr<LvContainer>;

} // namespace esp_brookesia::gui