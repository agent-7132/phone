/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_PHONE_HONE_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_phone_utils.hpp"
#include "esp_brookesia_phone_app.hpp"
#include "esp_brookesia_phone_display.hpp"

using namespace std;
using namespace esp_brookesia::gui;

namespace esp_brookesia::systems::phone {

/**
 * @brief Display类构造函数
 * 
 * 初始化显示管理对象，创建应用启动器并初始化其他组件为空。
 * 
 * @param core 核心上下文引用
 * @param data 显示配置数据引用
 */
Display::Display(base::Context &core, const Display::Data &data):
    base::Display(core, core.getData().display),
    _data(data),
    _app_launcher(core, data.app_launcher.data),
    _status_bar(nullptr),
    _navigation_bar(nullptr),
    _recents_screen(nullptr)
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
        ESP_UTILS_LOGE("Failed to delete");
    }
}

/**
 * @brief 初始化显示系统
 * 
 * 执行以下初始化步骤：
 * 1. 创建并初始化最近屏幕（如果启用）
 * 2. 创建并初始化状态栏（如果启用）
 * 3. 创建并初始化导航栏（如果启用）
 * 4. 初始化应用启动器
 * 
 * @return true 成功，false 失败
 */
bool Display::begin(void)
{
    lv_obj_t *main_screen_obj = _system_context.getDisplay().getMainScreenObject();
    lv_obj_t *system_screen_obj = _system_context.getDisplay().getSystemScreenObject();
    shared_ptr<StatusBar> status_bar = nullptr;
    shared_ptr<NavigationBar> navigation_bar = nullptr;
    shared_ptr<RecentsScreen> recents_screen = nullptr;

    ESP_UTILS_LOGD("Begin(@0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");

    // Recents Screen
    if (_data.flags.enable_recents_screen) {
        recents_screen = std::make_shared<RecentsScreen>(_system_context, _data.recents_screen.data);
        ESP_UTILS_CHECK_NULL_RETURN(recents_screen, false, "Create recents_screen failed");
        ESP_UTILS_CHECK_FALSE_RETURN(recents_screen->begin(system_screen_obj), false, "Begin recents_screen failed");
    }

    // Status bar
    if (_data.flags.enable_status_bar) {
        status_bar = std::make_shared<StatusBar>(_system_context, _data.status_bar.data,
                     _system_context.getManager().getAppFreeId(), _system_context.getManager().getAppFreeId());
        ESP_UTILS_CHECK_NULL_RETURN(status_bar, false, "Create status bar failed");
        ESP_UTILS_CHECK_FALSE_RETURN(status_bar->begin(system_screen_obj), false, "Begin status bar failed");
        ESP_UTILS_CHECK_FALSE_RETURN(status_bar->setVisualMode(_data.status_bar.visual_mode), false,
                                     "Status bar set visual mode failed");
    }

    // Navigation bar
    if (_data.flags.enable_navigation_bar) {
        navigation_bar = std::make_shared<NavigationBar>(_system_context, _data.navigation_bar.data);
        ESP_UTILS_CHECK_NULL_RETURN(navigation_bar, false, "Create navigation bar failed");
        ESP_UTILS_CHECK_FALSE_RETURN(navigation_bar->begin(system_screen_obj), false, "Begin navigation bar failed");
        ESP_UTILS_CHECK_FALSE_RETURN(navigation_bar->setVisualMode(_data.navigation_bar.visual_mode), false,
                                     "Navigation bar set visual mode failed");
    }

    // App launcher
    ESP_UTILS_CHECK_FALSE_RETURN(_app_launcher.begin(main_screen_obj), false, "Begin app launcher failed");

    _status_bar = status_bar;
    _navigation_bar = navigation_bar;
    _recents_screen = recents_screen;

    return true;
}

/**
 * @brief 销毁显示系统
 * 
 * 按逆序销毁各组件：状态栏 -> 导航栏 -> 最近屏幕 -> 应用启动器。
 * 
 * @return true 成功，false 失败
 */
bool Display::del(void)
{
    ESP_UTILS_LOGD("Delete(@0x%p)", this);

    if (!checkInitialized()) {
        return true;
    }

    if (_status_bar) {
        _status_bar.reset();
    }
    if (_navigation_bar) {
        _navigation_bar.reset();
    }
    if (_recents_screen) {
        _recents_screen.reset();
    }
    if (!_app_launcher.del()) {
        ESP_UTILS_LOGE("Delete app launcher failed");
    }

    return true;
}

/**
 * @brief 处理应用安装事件
 * 
 * 在应用安装时，向应用启动器添加应用图标。如果应用没有提供启动器图标，则使用默认图标。
 * 
 * @param app 应用对象指针
 * @return true 成功，false 失败
 */
bool Display::processAppInstall(base::App *app)
{
    App *phone_app = static_cast<App *>(app);
    phone::AppLauncherIcon::Info icon_info = {};

    ESP_UTILS_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");
    ESP_UTILS_LOGD("Process when app(%d) install", phone_app->getId());
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    // Process app launcher
    icon_info = (phone::AppLauncherIcon::Info) {
        phone_app->getName(), phone_app->getLauncherIcon(), phone_app->getId()
    };
    if (phone_app->getLauncherIcon().resource == nullptr) {
        ESP_UTILS_LOGW("No launcher icon provided, use default icon");
        icon_info.image = _data.app_launcher.default_image;
        phone_app->setLauncherIconImage(icon_info.image);
    }
    ESP_UTILS_CHECK_FALSE_RETURN(_app_launcher.addIcon(phone_app->getActiveConfig().app_launcher_page_index, icon_info),
                                 false, "Add launcher icon failed");

    return true;
}

/**
 * @brief 处理应用卸载事件
 * 
 * 在应用卸载时，从应用启动器中移除应用图标。
 * 
 * @param app 应用对象指针
 * @return true 成功，false 失败
 */
bool Display::processAppUninstall(base::App *app)
{
    App *phone_app = static_cast<App *>(app);

    ESP_UTILS_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");
    ESP_UTILS_LOGD("Process when app(%d) uninstall", phone_app->getId());
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    // Process app launcher
    ESP_UTILS_CHECK_FALSE_RETURN(_app_launcher.removeIcon(phone_app->getId()), false, "Remove launcher icon failed");

    return true;
}

/**
 * @brief 处理应用运行事件
 * 
 * 在应用运行时执行以下操作：
 * 1. 向状态栏添加应用图标（如果配置了）
 * 2. 更新状态栏显示模式
 * 3. 更新导航栏显示模式
 * 4. 向最近屏幕添加快照
 * 
 * @param app 应用对象指针
 * @return true 成功，false 失败
 */
bool Display::processAppRun(base::App *app)
{
    App *phone_app = static_cast<App *>(app);

    ESP_UTILS_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");
    ESP_UTILS_LOGD("Process when app(%d) run", phone_app->getId());
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    const App::Config &app_data = phone_app->getActiveConfig();
    // Process status bar
    if (_status_bar == nullptr) {
        ESP_UTILS_LOGD("No status_bar");
    } else {
        // Add status bar icon if needed
        if (app_data.status_icon_data.icon.image_num > 0) {
            if (app_data.flags.enable_status_icon_common_size) {
                ESP_UTILS_LOGD("Use common size for status icon");
                phone_app->_active_config.status_icon_data.size = _data.status_bar.data.icon_common_size;
            }
            ESP_UTILS_CHECK_FALSE_RETURN(
                StatusBar::calibrateIconData(_data.status_bar.data, *this, phone_app->_active_config.status_icon_data),
                false, "Calibrate status icon data failed"
            );
            ESP_UTILS_CHECK_FALSE_RETURN(
                _status_bar->addIcon(app_data.status_icon_data, app_data.status_icon_area_index, phone_app->getId()),
                false, "Add status icon failed"
            );
        }
        // Change visibility
        ESP_UTILS_CHECK_FALSE_RETURN(_status_bar->setVisualMode(app_data.status_bar_visual_mode), false,
                                     "Status bar set visual mode failed");
    }

    // Process navigation bar
    if (_navigation_bar == nullptr) {
        ESP_UTILS_LOGD("No navigation_bar");
    } else {
        // Change visibility
        ESP_UTILS_CHECK_FALSE_RETURN(_navigation_bar->setVisualMode(app_data.navigation_bar_visual_mode), false,
                                     "Navigation bar set visual mode failed");
    }

    // Process recents_screen
    if (_recents_screen == nullptr) {
        ESP_UTILS_LOGD("No recents_screen");
    } else {
        ESP_UTILS_LOGD("Add recents_screen snapshot");
        ESP_UTILS_CHECK_FALSE_RETURN(phone_app->updateRecentsScreenSnapshotConf(nullptr), false,
                                     "Update snapshot conf failed");
        ESP_UTILS_CHECK_FALSE_RETURN(_recents_screen->addSnapshot(phone_app->_recents_screen_snapshot_conf), false,
                                     "RecentsScreen add snapshot failed");
    }

    return true;
}

/**
 * @brief 处理应用恢复事件
 * 
 * 在应用恢复时更新状态栏和导航栏的显示模式。
 * 
 * @param app 应用对象指针
 * @return true 成功，false 失败
 */
bool Display::processAppResume(base::App *app)
{
    App *phone_app = static_cast<App *>(app);

    ESP_UTILS_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");
    ESP_UTILS_LOGD("Process when app(%d) resume", phone_app->getId());
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    const App::Config &app_data = phone_app->getActiveConfig();
    // Process status bar
    if (_status_bar == nullptr) {
        ESP_UTILS_LOGD("No status_bar");
    } else {
        // Change visibility
        ESP_UTILS_CHECK_FALSE_RETURN(_status_bar->setVisualMode(app_data.status_bar_visual_mode), false,
                                     "Status bar set visual mode failed");
    }

    // Process navigation bar
    if (_navigation_bar == nullptr) {
        ESP_UTILS_LOGD("No navigation_bar");
    } else {
        // Change visibility
        ESP_UTILS_CHECK_FALSE_RETURN(_navigation_bar->setVisualMode(app_data.navigation_bar_visual_mode), false,
                                     "Navigation bar set visual mode failed");
    }

    return true;
}

/**
 * @brief 处理应用关闭事件
 * 
 * 在应用关闭时执行以下操作：
 * 1. 从状态栏移除应用图标（如果存在）
 * 2. 从最近屏幕移除快照（如果存在）
 * 
 * @param app 应用对象指针
 * @return true 成功，false 失败
 */
bool Display::processAppClose(base::App *app)
{
    App *phone_app = static_cast<App *>(app);

    ESP_UTILS_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");
    ESP_UTILS_LOGD("Process when app(%d) close", phone_app->getId());
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    // Process status bar
    if (_status_bar == nullptr) {
        ESP_UTILS_LOGD("No status_bar");
    } else if (phone_app->getActiveConfig().status_icon_data.icon.image_num > 0) {
        // Remove status bar icon if created
        ESP_UTILS_CHECK_FALSE_RETURN(_status_bar->removeIcon(phone_app->getId()), false, "Remove status icon failed");
    }

    // Process recents_screen
    if (_recents_screen == nullptr) {
        ESP_UTILS_LOGD("No recents_screen");
    } else {
        if (_recents_screen->checkSnapshotExist(phone_app->getId())) {
            ESP_UTILS_CHECK_FALSE_RETURN(_recents_screen->removeSnapshot(phone_app->getId()), false,
                                         "Remove snapshot failed");
        }
    }

    return true;
}

/**
 * @brief 处理主屏幕加载事件
 * 
 * 更新状态栏和导航栏的显示模式为默认值，并加载主屏幕。
 * 
 * @return true 成功，false 失败
 */
bool Display::processMainScreenLoad(void)
{
    lv_obj_t *main_screen = _system_context.getDisplay().getMainScreen();

    ESP_UTILS_LOGD("Process when load display");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    // Process status bar
    if (_status_bar == nullptr) {
        ESP_UTILS_LOGD("No status_bar");
    } else {
        ESP_UTILS_CHECK_FALSE_RETURN(_status_bar->setVisualMode(_data.status_bar.visual_mode), false,
                                     "Status bar set visual mode failed");
    }

    // Process navigation bar
    if (_navigation_bar == nullptr) {
        ESP_UTILS_LOGD("No navigation_bar");
    } else {
        ESP_UTILS_CHECK_FALSE_RETURN(_navigation_bar->setVisualMode(_data.navigation_bar.visual_mode), false,
                                     "Navigation bar set visual mode failed");
    }

    ESP_UTILS_CHECK_FALSE_RETURN(checkLvObjIsValid(main_screen), false, "Invalid main screen");
    lv_scr_load(main_screen);

    return true;
}

/**
 * @brief 获取应用可视区域
 * 
 * 根据状态栏和导航栏的状态计算应用的可用可视区域。
 * 
 * @param app 应用对象指针
 * @param app_visual_area 应用可视区域（输出参数）
 * @return true 成功，false 失败
 */
bool Display::getAppVisualArea(base::App *app, lv_area_t &app_visual_area) const
{
    App *phone_app = static_cast<App *>(app);

    ESP_UTILS_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");

    gui::StyleSize display_size = {};
    ESP_UTILS_CHECK_FALSE_RETURN(_system_context.getDisplaySize(display_size), false, "Get display size failed");

    lv_area_t visual_area = {
        .x1 = 0,
        .y1 = 0,
        .x2 = (lv_coord_t)(display_size.width - 1),
        .y2 = (lv_coord_t)(display_size.height - 1),
    };
    const App::Config &app_data = phone_app->getActiveConfig();

    // Process status bar
    if ((_status_bar != nullptr) && (app_data.status_bar_visual_mode == StatusBar::VisualMode::SHOW_FIXED) &&
            (_data.status_bar.data.main.background_color.opacity == LV_OPA_COVER)) {
        visual_area.y1 = _data.status_bar.data.main.size.height;
    }

    // Process navigation bar
    if ((_navigation_bar != nullptr) &&
            (app_data.navigation_bar_visual_mode == NavigationBar::VisualMode::SHOW_FIXED)) {
        visual_area.y2 -= _data.navigation_bar.data.main.size.height;
    }

    app_visual_area = visual_area;

    return true;
}

/**
 * @brief 处理最近屏幕显示事件
 * 
 * 更新状态栏和导航栏的显示模式为最近屏幕配置，并显示最近屏幕。
 * 
 * @return true 成功，false 失败
 */
bool Display::processRecentsScreenShow(void)
{
    ESP_UTILS_LOGD("Process when show recents_screen");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_NULL_RETURN(_recents_screen, false, "No recents_screen");

    // Process status bar
    if (_status_bar == nullptr) {
        ESP_UTILS_LOGD("No status_bar");
    } else {
        ESP_UTILS_CHECK_FALSE_RETURN(_status_bar->setVisualMode(_data.recents_screen.status_bar_visual_mode), false,
                                     "Status bar set visual mode failed");
    }

    // Process navigation bar
    if (_navigation_bar == nullptr) {
        ESP_UTILS_LOGD("No navigation_bar");
    } else {
        ESP_UTILS_CHECK_FALSE_RETURN(_navigation_bar->setVisualMode(_data.recents_screen.navigation_bar_visual_mode),
                                     false, "Navigation bar set visual mode failed");
    }

    ESP_UTILS_CHECK_FALSE_RETURN(_recents_screen->setVisible(true), false, "RecentsScreen show failed");

    return true;
}

/**
 * @brief 校准显示配置数据
 * 
 * 根据屏幕尺寸校准各组件的配置数据，包括：
 * 1. 初始化灵活尺寸组件的大小
 * 2. 校准状态栏数据并调整灵活尺寸组件
 * 3. 校准导航栏数据并调整灵活尺寸组件
 * 4. 校准最近屏幕数据
 * 5. 校准应用启动器数据
 * 
 * @param screen_size 屏幕尺寸
 * @param data 待校准的显示配置数据
 * @return true 成功，false 失败
 */
bool Display::calibrateData(const gui::StyleSize &screen_size, Display::Data &data)
{
    ESP_UTILS_LOGD("Calibrate data");

    // Initialize the size of flex widgets
    if (data.flags.enable_app_launcher_flex_size) {
        data.app_launcher.data.main.y_start = 0;
        data.app_launcher.data.main.size.flags.enable_height_percent = 0;
        data.app_launcher.data.main.size.height = screen_size.height;
    }
    if (data.flags.enable_recents_screen && data.flags.enable_recents_screen_flex_size) {
        data.recents_screen.data.main.y_start = 0;
        data.recents_screen.data.main.size.flags.enable_height_percent = 0;
        data.recents_screen.data.main.size.height = screen_size.height;
    }

    // Status bar
    if (data.flags.enable_status_bar) {
        ESP_UTILS_CHECK_FALSE_RETURN(StatusBar::calibrateData(screen_size, *this, data.status_bar.data),
                                     false, "Calibrate status bar data failed");
        if (data.flags.enable_app_launcher_flex_size &&
                (data.status_bar.visual_mode == StatusBar::VisualMode::SHOW_FIXED)) {
            data.app_launcher.data.main.y_start += data.status_bar.data.main.size.height;
            data.app_launcher.data.main.size.height -= data.status_bar.data.main.size.height;
        }
        if (data.flags.enable_recents_screen && data.flags.enable_recents_screen_flex_size &&
                (data.recents_screen.status_bar_visual_mode == StatusBar::VisualMode::SHOW_FIXED)) {
            data.recents_screen.data.main.y_start += data.status_bar.data.main.size.height;
            data.recents_screen.data.main.size.height -= data.status_bar.data.main.size.height;
        }
    }
    // Navigation bar
    if (data.flags.enable_navigation_bar) {
        ESP_UTILS_CHECK_FALSE_RETURN(NavigationBar::calibrateData(screen_size, *this, data.navigation_bar.data),
                                     false, "Calibrate navigation bar data failed");
        if (data.flags.enable_app_launcher_flex_size &&
                (data.navigation_bar.visual_mode == NavigationBar::VisualMode::SHOW_FIXED)) {
            ESP_UTILS_CHECK_VALUE_RETURN(data.app_launcher.data.main.y_start + data.navigation_bar.data.main.size.height,
                                         1, screen_size.height, false, "Invalid app launcher height flex");
            data.app_launcher.data.main.size.height -= data.navigation_bar.data.main.size.height;
        }
        if (data.flags.enable_recents_screen && data.flags.enable_recents_screen_flex_size &&
                (data.recents_screen.navigation_bar_visual_mode == NavigationBar::VisualMode::SHOW_FIXED)) {
            ESP_UTILS_CHECK_VALUE_RETURN(data.recents_screen.data.main.y_start + data.recents_screen.data.main.size.height,
                                         1, screen_size.height, false, "Invalid app launcher height flex");
            data.recents_screen.data.main.size.height -= data.navigation_bar.data.main.size.height;
        }
    }
    // Recents Screen
    if (data.flags.enable_recents_screen) {
        ESP_UTILS_CHECK_FALSE_RETURN(RecentsScreen::calibrateData(screen_size, *this, data.recents_screen.data),
                                     false, "Calibrate recents_screen data failed");
    }
    // App launcher
    ESP_UTILS_CHECK_FALSE_RETURN(AppLauncher::calibrateData(screen_size, *this, data.app_launcher.data),
                                 false, "Calibrate app launcher data failed");

    return true;
}

} // namespace esp_brookesia::systems::phone