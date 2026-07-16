/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <list>
#include <memory>
#include "esp_brookesia_systems_internal.h"
#include "gui/style/esp_brookesia_gui_stylesheet_manager.hpp"
#include "esp_brookesia_phone_display.hpp"
#include "esp_brookesia_phone_manager.hpp"
#include "esp_brookesia_phone_app.hpp"

namespace esp_brookesia::systems::phone {

/**
 * @struct Stylesheet
 * @brief 手机系统样式表结构
 * 
 * 包含手机系统各组件的配置数据，用于统一管理和切换系统外观。
 */
struct Stylesheet {
    base::Context::Data core;       /*!< 核心上下文配置数据 */
    Display::Data display;          /*!< 显示配置数据 */
    Manager::Data manager;          /*!< 管理器配置数据 */
};

/**
 * @brief 样式表管理器类型定义
 * 
 * 使用泛型StylesheetManager管理Phone系统的样式表。
 */
using StylesheetManager = gui::StylesheetManager<Stylesheet>;

/**
 * @class Phone
 * @brief 手机系统主类
 * 
 * Phone类是整个手机系统的核心入口，继承自base::Context和StylesheetManager。
 * 它整合了显示管理(Display)和应用管理(Manager)，提供应用安装、样式表管理等功能。
 */
class Phone: public base::Context, public StylesheetManager {
public:
    /**
     * @brief 构造函数
     * 
     * @param display LVGL显示设备指针，默认为nullptr（使用默认显示）
     */
    Phone(lv_display_t *display = nullptr);

    /**
     * @brief 析构函数
     */
    ~Phone();

    /**
     * @brief 删除拷贝构造函数和赋值运算符，防止对象拷贝
     */
    Phone(const Phone &) = delete;
    Phone(Phone &&) = delete;
    Phone &operator=(const Phone &) = delete;
    Phone &operator=(Phone &&) = delete;

    /**
     * @brief 安装应用（引用版本）
     * 
     * @param app 应用对象引用
     * @return int 应用ID，失败返回-1
     */
    int installApp(App &app);

    /**
     * @brief 安装应用（指针版本）
     * 
     * @param app 应用对象指针
     * @return int 应用ID，失败返回-1
     */
    int installApp(App *app);

    /**
     * @brief 卸载应用（引用版本）
     * 
     * @param app 应用对象引用
     * @return true 成功，false 失败
     */
    bool uninstallApp(App &app);

    /**
     * @brief 卸载应用（指针版本）
     * 
     * @param app 应用对象指针
     * @return true 成功，false 失败
     */
    bool uninstallApp(App *app);

    /**
     * @brief 卸载应用（ID版本）
     * 
     * @param id 应用ID
     * @return true 成功，false 失败
     */
    bool uninstallApp(int id);

    /**
     * @brief 初始化手机系统
     * 
     * 执行以下初始化步骤：
     * 1. 添加默认样式表（如果未添加）
     * 2. 激活默认样式表（如果未激活）
     * 3. 初始化核心上下文、显示和管理器
     * 
     * @return true 成功，false 失败
     */
    bool begin(void);

    /**
     * @brief 销毁手机系统
     * 
     * 按逆序销毁管理器、显示、样式表管理器和核心上下文。
     * 
     * @return true 成功，false 失败
     */
    bool del(void);

    /**
     * @brief 添加样式表（引用版本）
     * 
     * @param stylesheet 样式表引用
     * @return true 成功，false 失败
     */
    bool addStylesheet(const Stylesheet &stylesheet);

    /**
     * @brief 添加样式表（指针版本）
     * 
     * @param stylesheet 样式表指针
     * @return true 成功，false 失败
     */
    bool addStylesheet(const Stylesheet *stylesheet);

    /**
     * @brief 激活样式表（引用版本）
     * 
     * 激活指定样式表并发送数据更新事件。
     * 
     * @param stylesheet 样式表引用
     * @return true 成功，false 失败
     */
    bool activateStylesheet(const Stylesheet &stylesheet);

    /**
     * @brief 激活样式表（指针版本）
     * 
     * @param stylesheet 样式表指针
     * @return true 成功，false 失败
     */
    bool activateStylesheet(const Stylesheet *stylesheet);

    /**
     * @brief 校准屏幕尺寸
     * 
     * 根据显示设备尺寸校准目标尺寸。
     * 
     * @param size 目标尺寸（将被校准）
     * @return true 成功，false 失败
     */
    bool calibrateScreenSize(gui::StyleSize &size) override;

    /**
     * @brief 获取显示管理对象
     * 
     * @return Display& 显示管理对象引用
     */
    Display &getDisplay(void)
    {
        return _display;
    }

    /**
     * @brief 获取管理器对象
     * 
     * @return Manager& 管理器对象引用
     */
    Manager &getManager(void)
    {
        return _manager;
    }

private:
    /**
     * @brief 校准样式表数据
     * 
     * 校准样式表中的核心数据、显示数据和管理器数据。
     * 
     * @param screen_size 屏幕尺寸
     * @param stylesheet 待校准的样式表
     * @return true 成功，false 失败
     */
    bool calibrateStylesheet(const gui::StyleSize &screen_size, Stylesheet &stylesheet) override;

    Display _display;   /*!< 显示管理对象 */
    Manager _manager;   /*!< 应用管理器对象 */

    /**
     * @brief 默认深色样式表
     */
    static const Stylesheet _default_stylesheet_dark;
};

} // namespace esp_brookesia::systems::phone

/**
 * @brief 向后兼容性定义
 */
using ESP_Brookesia_PhoneStylesheet_t [[deprecated("Use `esp_brookesia::systems::phone::Stylesheet` instead")]] =
    esp_brookesia::systems::phone::Stylesheet;
using ESP_Brookesia_PhoneStylesheetManager [[deprecated("Use `esp_brookesia::systems::phone::StylesheetManager` instead")]] =
    esp_brookesia::systems::phone::StylesheetManager;
using ESP_Brookesia_Phone [[deprecated("Use `esp_brookesia::systems::phone::Phone` instead")]] =
    esp_brookesia::systems::phone::Phone;