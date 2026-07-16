/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_PHONE_PHONE_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_phone_utils.hpp"
#include "stylesheets/esp_brookesia_phone_stylesheets.hpp"
#include "esp_brookesia_phone.hpp"

using namespace std;
using namespace esp_brookesia::gui;

namespace esp_brookesia::systems::phone {

/**
 * @brief 默认深色样式表初始化
 */
const Stylesheet Phone::_default_stylesheet_dark = ESP_BROOKESIA_PHONE_DEFAULT_DARK_STYLESHEET();

/**
 * @brief Phone类构造函数
 * 
 * 初始化Phone对象，依次初始化：
 * 1. base::Context（使用当前激活的样式表核心数据）
 * 2. StylesheetManager
 * 3. Display（使用当前激活的样式表显示数据）
 * 4. Manager（使用当前激活的样式表管理器数据）
 * 
 * @param display LVGL显示设备指针
 */
Phone::Phone(lv_display_t *display):
    base::Context(_active_stylesheet.core, _display, _manager, display),
    StylesheetManager(),
    _display(*this, _active_stylesheet.display),
    _manager(*this, _display, _active_stylesheet.manager)
{
}

/**
 * @brief Phone类析构函数
 * 
 * 调用del()方法清理资源。
 */
Phone::~Phone()
{
    ESP_UTILS_LOGD("Destroy phone(@0x%p)", this);
    if (!del()) {
        ESP_UTILS_LOGE("Delete failed");
    }
}

/**
 * @brief 初始化手机系统
 * 
 * 执行以下初始化步骤：
 * 1. 检查是否已添加样式表，若未添加则添加默认深色样式表
 * 2. 检查是否已激活样式表，若未激活则根据屏幕尺寸查找并激活
 * 3. 依次初始化核心上下文、显示和管理器
 * 
 * @return true 成功，false 失败
 */
bool Phone::begin(void)
{
    bool ret = true;
    const Stylesheet *default_find_data = nullptr;
    StyleSize display_size = {};

    ESP_UTILS_LOGD("Begin phone(@0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(!checkCoreInitialized(), false, "Already initialized");

    // Check if any phone stylesheet is added, if not, add default stylesheet
    if (getStylesheetCount() == 0) {
        ESP_UTILS_LOGW("No phone stylesheet is added, adding default dark stylesheet(%s)",
                       _default_stylesheet_dark.core.name);
        ESP_UTILS_CHECK_FALSE_GOTO(ret = addStylesheet(_default_stylesheet_dark), end,
                                   "Failed to add default stylesheet");
    }
    // Check if any phone stylesheet is activated, if not, activate default stylesheet
    if (_active_stylesheet.core.name == nullptr) {
        ESP_UTILS_CHECK_NULL_RETURN(_display_device, false, "Invalid display");
        display_size.width = lv_disp_get_hor_res(_display_device);
        display_size.height = lv_disp_get_ver_res(_display_device);

        ESP_UTILS_LOGW("No phone stylesheet is activated, try to find first stylesheet with display size(%dx%d)",
                       display_size.width, display_size.height);
        default_find_data = getStylesheet(display_size);
        ESP_UTILS_CHECK_NULL_GOTO(default_find_data, end, "Failed to get default stylesheet");

        ret = activateStylesheet(*default_find_data);
        ESP_UTILS_CHECK_FALSE_GOTO(ret, end, "Failed to activate default stylesheet");
    }

    ESP_UTILS_CHECK_FALSE_GOTO(ret = base::Context::begin(), end, "Failed to begin core");
    ESP_UTILS_CHECK_FALSE_GOTO(ret = _display.begin(), end, "Failed to begin display");
    ESP_UTILS_CHECK_FALSE_GOTO(ret = _manager.begin(), end, "Failed to begin manager");

end:
    return ret;
}

/**
 * @brief 销毁手机系统
 * 
 * 按逆序销毁组件：管理器 -> 显示 -> 样式表管理器 -> 核心上下文。
 * 
 * @return true 成功，false 失败
 */
bool Phone::del(void)
{
    ESP_UTILS_LOGD("Delete(@0x%p)", this);

    if (!checkCoreInitialized()) {
        return true;
    }

    if (!_manager.del()) {
        ESP_UTILS_LOGE("Delete manager failed");
    }
    if (!_display.del()) {
        ESP_UTILS_LOGE("Delete display failed");
    }
    if (!StylesheetManager::del()) {
        ESP_UTILS_LOGE("Delete stylesheet manager failed");
    }
    if (!base::Context::del()) {
        ESP_UTILS_LOGE("Delete core failed");
    }

    return true;
}

/**
 * @brief 安装应用（引用版本）
 * 
 * 委托给base::Context::getManager().installApp()处理。
 * 
 * @param app 应用对象引用
 * @return int 应用ID，失败返回-1
 */
int Phone::installApp(App &app)
{
    return base::Context::getManager().installApp(app);
}

/**
 * @brief 安装应用（指针版本）
 * 
 * 委托给base::Context::getManager().installApp()处理。
 * 
 * @param app 应用对象指针
 * @return int 应用ID，失败返回-1
 */
int Phone::installApp(App *app)
{
    return base::Context::getManager().installApp(app);
}

/**
 * @brief 卸载应用（引用版本）
 * 
 * 委托给base::Context::getManager().uninstallApp()处理。
 * 
 * @param app 应用对象引用
 * @return true 成功，false 失败
 */
bool Phone::uninstallApp(App &app)
{
    return base::Context::getManager().uninstallApp(app);
}

/**
 * @brief 卸载应用（指针版本）
 * 
 * 委托给base::Context::getManager().uninstallApp()处理。
 * 
 * @param app 应用对象指针
 * @return true 成功，false 失败
 */
bool Phone::uninstallApp(App *app)
{
    return base::Context::getManager().uninstallApp(app);
}

/**
 * @brief 卸载应用（ID版本）
 * 
 * 委托给base::Context::getManager().uninstallApp()处理。
 * 
 * @param id 应用ID
 * @return true 成功，false 失败
 */
bool Phone::uninstallApp(int id)
{
    return base::Context::getManager().uninstallApp(id);
}

/**
 * @brief 添加样式表（引用版本）
 * 
 * 使用样式表名称和屏幕尺寸作为键添加样式表。
 * 
 * @param stylesheet 样式表引用
 * @return true 成功，false 失败
 */
bool Phone::addStylesheet(const Stylesheet &stylesheet)
{
    ESP_UTILS_LOGD("Add phone(0x%p) stylesheet", this);

    ESP_UTILS_CHECK_FALSE_RETURN(
        StylesheetManager::addStylesheet(stylesheet.core.name, stylesheet.core.screen_size, stylesheet),
        false, "Failed to add phone stylesheet"
    );

    return true;
}

/**
 * @brief 添加样式表（指针版本）
 * 
 * 调用引用版本的addStylesheet方法。
 * 
 * @param stylesheet 样式表指针
 * @return true 成功，false 失败
 */
bool Phone::addStylesheet(const Stylesheet *stylesheet)
{
    ESP_UTILS_LOGD("Add phone(0x%p) stylesheet", this);

    ESP_UTILS_CHECK_FALSE_RETURN(addStylesheet(*stylesheet), false, "Failed to add phone stylesheet");

    return true;
}

/**
 * @brief 激活样式表（引用版本）
 * 
 * 使用样式表名称和屏幕尺寸激活样式表，如果系统已初始化则发送数据更新事件。
 * 
 * @param stylesheet 样式表引用
 * @return true 成功，false 失败
 */
bool Phone::activateStylesheet(const Stylesheet &stylesheet)
{
    ESP_UTILS_LOGD("Activate phone(0x%p) stylesheet", this);

    ESP_UTILS_CHECK_FALSE_RETURN(
        StylesheetManager::activateStylesheet(stylesheet.core.name, stylesheet.core.screen_size),
        false, "Failed to activate phone stylesheet"
    );

    if (checkCoreInitialized() && !sendDataUpdateEvent()) {
        ESP_UTILS_LOGE("Send update data event failed");
    }

    return true;
}

/**
 * @brief 激活样式表（指针版本）
 * 
 * 调用引用版本的activateStylesheet方法。
 * 
 * @param stylesheet 样式表指针
 * @return true 成功，false 失败
 */
bool Phone::activateStylesheet(const Stylesheet *stylesheet)
{
    ESP_UTILS_LOGD("Activate phone(0x%p) stylesheet", this);

    ESP_UTILS_CHECK_FALSE_RETURN(activateStylesheet(*stylesheet), false, "Failed to activate phone stylesheet");

    return true;
}

/**
 * @brief 校准样式表数据
 * 
 * 执行以下校准步骤：
 * 1. 校准核心上下文数据
 * 2. 检查并处理手势和最近屏幕的兼容性
 * 3. 校准显示数据
 * 4. 校准管理器数据
 * 
 * @param screen_size 屏幕尺寸
 * @param stylesheet 待校准的样式表
 * @return true 成功，false 失败
 */
bool Phone::calibrateStylesheet(const gui::StyleSize &screen_size, Stylesheet &stylesheet)
{
    ESP_UTILS_LOGD("Calibrate phone(0x%p) stylesheet", this);

    // Core
    ESP_UTILS_CHECK_FALSE_RETURN(calibrateCoreData(stylesheet.core), false, "Invalid core data");

    // Display
    if (!_active_stylesheet.manager.flags.enable_gesture && _active_stylesheet.display.flags.enable_recents_screen) {
        ESP_UTILS_LOGW("Gesture is disabled, but recents_screen is enabled, disable recents_screen automatically");
        _active_stylesheet.display.flags.enable_recents_screen = 0;
    }
    ESP_UTILS_CHECK_FALSE_RETURN(_display.calibrateData(screen_size, stylesheet.display), false, "Invalid display data");
    ESP_UTILS_CHECK_FALSE_RETURN(_manager.calibrateData(screen_size, _display, stylesheet.manager), false,
                                 "Invalid manager data");

    return true;
}

/**
 * @brief 校准屏幕尺寸
 * 
 * 获取显示设备尺寸并调用Display的calibrateCoreObjectSize方法校准目标尺寸。
 * 
 * @param size 目标尺寸（将被校准）
 * @return true 成功，false 失败
 */
bool Phone::calibrateScreenSize(gui::StyleSize &size)
{
    ESP_UTILS_LOGD("Calibrate phone(0x%p) screen size", this);

    gui::StyleSize display_size = {};
    ESP_UTILS_CHECK_FALSE_RETURN(getDisplaySize(display_size), false, "Get display size failed");
    ESP_UTILS_CHECK_FALSE_RETURN(base::Context::getDisplay().calibrateCoreObjectSize(display_size, size), false, "Invalid screen size");

    return true;
}

} // namespace esp_brookesia::systems::phone