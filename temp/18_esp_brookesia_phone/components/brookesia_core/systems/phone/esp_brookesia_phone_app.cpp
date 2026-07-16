/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_PHONE_APP_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_phone_utils.hpp"
#include "esp_brookesia_phone.hpp"
#include "esp_brookesia_phone_app.hpp"

using namespace std;
using namespace esp_brookesia::gui;

namespace esp_brookesia::systems::phone {

/**
 * @brief App类构造函数（完整配置版本）
 * 
 * 使用核心配置和手机配置初始化应用。
 * 
 * @param core_data 核心应用配置
 * @param phone_data 手机应用配置
 */
App::App(const base::App::Config &core_data, const Config &phone_data):
    base::App(core_data),
    _init_config(phone_data),
    _recents_screen_snapshot_conf{}
{
}

/**
 * @brief App类构造函数（带状态栏和导航栏配置）
 * 
 * 使用简单构造函数初始化手机应用配置。
 * 
 * @param name 应用名称
 * @param launcher_icon 启动器图标资源
 * @param use_default_screen 是否使用默认屏幕
 * @param use_status_bar 是否启用状态栏
 * @param use_navigation_bar 是否启用导航栏
 */
App::App(const char *name, const void *launcher_icon, bool use_default_screen,
         bool use_status_bar, bool use_navigation_bar):
    base::App(name, launcher_icon, use_default_screen),
    _init_config(Config::SIMPLE_CONSTRUCTOR(launcher_icon, use_status_bar, use_navigation_bar)),
    _recents_screen_snapshot_conf{}
{
}

/**
 * @brief App类构造函数（简化版本）
 * 
 * 默认启用状态栏，禁用导航栏。
 * 
 * @param name 应用名称
 * @param launcher_icon 启动器图标资源
 * @param use_default_screen 是否使用默认屏幕
 */
App::App(const char *name, const void *launcher_icon, bool use_default_screen):
    base::App(name, launcher_icon, use_default_screen),
    _init_config(Config::SIMPLE_CONSTRUCTOR(launcher_icon, true, false)),
    _recents_screen_snapshot_conf{}
{
}

/**
 * @brief App类析构函数
 * 
 * 如果应用已初始化，则自动卸载应用。
 */
App::~App()
{
    ESP_UTILS_LOGD("Destroy(@0x%p)", this);

    // Uninstall the app if it is initialized
    if (checkInitialized()) {
        if (!getSystem()->getManager().uninstallApp(this)) {
            ESP_UTILS_LOGE("Uninstall app failed");
        }
    }
}

/**
 * @brief 设置状态栏图标状态
 * 
 * 更新应用在状态栏中的图标显示状态。
 * 
 * @param state 图标状态值，应小于图标总数
 * @return true 成功，false 失败
 */
bool App::setStatusIconState(int state)
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "App is not initialized");

    StatusBar *status_bar = getSystem()->getDisplay().getStatusBar();
    ESP_UTILS_CHECK_NULL_RETURN(status_bar, false, "Status bar is invalid");

    ESP_UTILS_CHECK_FALSE_RETURN(status_bar->setIconState(getId(), state), false, "Failed to set status icon state");

    return true;
}

/**
 * @brief 获取手机系统对象
 * 
 * 将系统上下文转换为Phone类型并返回。
 * 
 * @return Phone* 手机系统对象指针
 */
Phone *App::getSystem(void)
{
    return static_cast<Phone *>(getSystemContext());
}

/**
 * @brief 应用初始化额外逻辑
 * 
 * 执行手机应用特有的初始化逻辑：
 * 1. 复制初始配置到活动配置
 * 2. 检查导航栏和手势配置的兼容性
 * 3. 检查状态栏图标配置
 * 
 * @return true 成功，false 失败
 */
bool App::beginExtra(void)
{
    ESP_UTILS_LOGD("Begin extra(@0x%p)", this);

    NavigationBar *navigation_bar = getSystem()->getDisplay().getNavigationBar();
    Gesture *gesture = getSystem()->getManager().getGesture();

    _active_config = _init_config;

    // Check navigation bar and gesture
    if ((_active_config.navigation_bar_visual_mode != NavigationBar::VisualMode::HIDE) &&
            (navigation_bar == nullptr)) {
        ESP_UTILS_LOGE("Navigation bar is enabled but not provided, disable it");
        _active_config.navigation_bar_visual_mode = NavigationBar::VisualMode::HIDE;
    }
    if (_active_config.flags.enable_navigation_gesture && (gesture == nullptr)) {
        ESP_UTILS_LOGE("Navigation gesture is enabled but not provided, disable it");
        _active_config.flags.enable_navigation_gesture = false;
    }
    if ((_active_config.navigation_bar_visual_mode == NavigationBar::VisualMode::SHOW_FIXED) &&
            _active_config.flags.enable_navigation_gesture) {
        ESP_UTILS_LOGW("Both navigation bar(fixed) and gesture are enabled, only bar will be used");
        _active_config.flags.enable_navigation_gesture = false;
    }

    // Check status icon
    if ((_active_config.status_icon_data.icon.image_num > 0) &&
            (_active_config.status_icon_data.icon.images[0].resource == nullptr)) {
        ESP_UTILS_LOGW("No status icon provided, use launcher icon");
        _active_config.status_icon_data.icon.images[0].resource = getLauncherIcon().resource;
    }

    return true;
}

/**
 * @brief 应用销毁额外逻辑
 * 
 * 清空活动配置和最近屏幕快照配置。
 * 
 * @return true 成功，false 失败
 */
bool App::delExtra(void)
{
    ESP_UTILS_LOGD("Delete extra(@0x%p)", this);

    _active_config = {};
    _recents_screen_snapshot_conf = {};

    return true;
}

/**
 * @brief 更新最近屏幕快照配置
 * 
 * 更新应用在最近屏幕中的快照配置，包括名称、图标和快照图像。
 * 
 * @param image_resource 快照图像资源，若为nullptr则使用启动器图标
 * @return true 成功，false 失败
 */
bool App::updateRecentsScreenSnapshotConf(const void *image_resource)
{
    ESP_UTILS_LOGD("Update recents_screen snapshot conf");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "App is not initialized");

    _recents_screen_snapshot_conf = {
        .name = getName(),
        .icon_image_resource = getLauncherIcon().resource,
        .snapshot_image_resource = (image_resource == nullptr) ? getLauncherIcon().resource : image_resource,
        .id = getId()
    };

    return true;
}

} // namespace esp_brookesia::systems::phone