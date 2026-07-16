/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "systems/base/esp_brookesia_base_context.hpp"
#include "systems/phone/widgets/status_bar/esp_brookesia_status_bar.hpp"
#include "systems/phone/widgets/navigation_bar/esp_brookesia_navigation_bar.hpp"
#include "systems/phone/widgets/app_launcher/esp_brookesia_app_launcher.hpp"
#include "systems/phone/widgets/recents_screen/esp_brookesia_recents_screen.hpp"

namespace esp_brookesia::systems::phone {

/**
 * @class Display
 * @brief 手机系统显示管理类
 * 
 * Display类继承自base::Display，负责管理手机系统的各种UI组件，包括状态栏、导航栏、应用启动器和最近屏幕。
 * 它处理应用安装、卸载、运行、恢复和关闭等事件，并管理屏幕加载和应用可视区域。
 */
class Display: public base::Display {
public:
    friend class Phone;
    friend class Manager;

    /**
     * @struct Data
     * @brief 显示配置数据结构
     * 
     * 包含手机系统各UI组件的配置信息和启用标志。
     */
    struct Data {
        /**
         * @brief 状态栏配置
         */
        struct {
            StatusBar::Data data;               /*!< 状态栏数据 */
            StatusBar::VisualMode visual_mode;  /*!< 状态栏显示模式 */
        } status_bar;

        /**
         * @brief 导航栏配置
         */
        struct {
            NavigationBar::Data data;               /*!< 导航栏数据 */
            NavigationBar::VisualMode visual_mode;  /*!< 导航栏显示模式 */
        } navigation_bar;

        /**
         * @brief 应用启动器配置
         */
        struct {
            AppLauncherData data;                /*!< 应用启动器数据 */
            gui::StyleImage default_image;       /*!< 默认图标图像 */
        } app_launcher;

        /**
         * @brief 最近屏幕配置
         */
        struct {
            RecentsScreen::Data data;                    /*!< 最近屏幕数据 */
            StatusBar::VisualMode status_bar_visual_mode;      /*!< 最近屏幕中状态栏显示模式 */
            NavigationBar::VisualMode navigation_bar_visual_mode;  /*!< 最近屏幕中导航栏显示模式 */
        } recents_screen;

        /**
         * @brief 功能启用标志位
         */
        struct {
            uint8_t enable_status_bar: 1;                          /*!< 启用状态栏 */
            uint8_t enable_navigation_bar: 1;                      /*!< 启用导航栏 */
            uint8_t enable_app_launcher_flex_size: 1;              /*!< 启用应用启动器灵活尺寸 */
            uint8_t enable_recents_screen: 1;                      /*!< 启用最近屏幕 */
            uint8_t enable_recents_screen_flex_size: 1;            /*!< 启用最近屏幕灵活尺寸 */
            uint8_t enable_recents_screen_hide_when_no_snapshot: 1; /*!< 无快照时隐藏最近屏幕(已废弃，使用manager中的标志) */
        } flags;
    };

    /**
     * @brief 构造函数
     * 
     * @param core 核心上下文引用
     * @param data 显示配置数据引用
     */
    Display(base::Context &core, const Data &data);

    /**
     * @brief 析构函数
     */
    ~Display();

    /**
     * @brief 检查初始化状态
     * 
     * 通过检查应用启动器的初始化状态来判断显示系统是否已初始化。
     * 
     * @return true 已初始化，false 未初始化
     */
    bool checkInitialized(void) const
    {
        return  _app_launcher.checkInitialized();
    }

    /**
     * @brief 获取显示配置数据
     * 
     * @return const Data& 显示配置数据引用
     */
    const Data &getData(void) const
    {
        return _data;
    }

    /**
     * @brief 获取状态栏对象
     * 
     * @return StatusBar* 状态栏指针，若未启用则返回nullptr
     */
    StatusBar *getStatusBar(void)
    {
        return _status_bar.get();
    }

    /**
     * @brief 获取导航栏对象
     * 
     * @return NavigationBar* 导航栏指针，若未启用则返回nullptr
     */
    NavigationBar *getNavigationBar(void)
    {
        return _navigation_bar.get();
    }

    /**
     * @brief 获取最近屏幕对象
     * 
     * @return RecentsScreen* 最近屏幕指针，若未启用则返回nullptr
     */
    RecentsScreen *getRecentsScreen(void)
    {
        return _recents_screen.get();
    }

    /**
     * @brief 获取应用启动器对象
     * 
     * @return AppLauncher* 应用启动器指针
     */
    AppLauncher *getAppLauncher(void)
    {
        return &_app_launcher;
    }

    /**
     * @brief 校准显示配置数据
     * 
     * 根据屏幕尺寸校准各组件的配置数据。
     * 
     * @param screen_size 屏幕尺寸
     * @param data 待校准的显示配置数据
     * @return true 成功，false 失败
     */
    bool calibrateData(const gui::StyleSize &screen_size, Data &data);

private:
    /**
     * @brief 初始化显示系统
     * 
     * 创建并初始化各UI组件。
     * 
     * @return true 成功，false 失败
     */
    bool begin(void);

    /**
     * @brief 销毁显示系统
     * 
     * 销毁各UI组件并释放资源。
     * 
     * @return true 成功，false 失败
     */
    bool del(void);

    /**
     * @brief 处理应用安装事件
     * 
     * 在应用安装时更新应用启动器。
     * 
     * @param app 应用对象指针
     * @return true 成功，false 失败
     */
    bool processAppInstall(base::App *app) override;

    /**
     * @brief 处理应用卸载事件
     * 
     * 在应用卸载时从应用启动器中移除。
     * 
     * @param app 应用对象指针
     * @return true 成功，false 失败
     */
    bool processAppUninstall(base::App *app) override;

    /**
     * @brief 处理应用运行事件
     * 
     * 在应用运行时切换到应用屏幕。
     * 
     * @param app 应用对象指针
     * @return true 成功，false 失败
     */
    bool processAppRun(base::App *app) override;

    /**
     * @brief 处理应用恢复事件
     * 
     * 在应用恢复时更新UI状态。
     * 
     * @param app 应用对象指针
     * @return true 成功，false 失败
     */
    bool processAppResume(base::App *app) override;

    /**
     * @brief 处理应用关闭事件
     * 
     * 在应用关闭时切换回主屏幕。
     * 
     * @param app 应用对象指针
     * @return true 成功，false 失败
     */
    bool processAppClose(base::App *app) override;

    /**
     * @brief 处理主屏幕加载事件
     * 
     * 加载应用启动器到主屏幕。
     * 
     * @return true 成功，false 失败
     */
    bool processMainScreenLoad(void) override;

    /**
     * @brief 获取应用可视区域
     * 
     * 根据状态栏和导航栏的状态计算应用的可用可视区域。
     * 
     * @param app 应用对象指针
     * @param app_visual_area 应用可视区域（输出参数）
     * @return true 成功，false 失败
     */
    bool getAppVisualArea(base::App *app, lv_area_t &app_visual_area) const override;

    /**
     * @brief 处理最近屏幕显示事件
     * 
     * 显示最近屏幕并更新状态栏和导航栏的显示模式。
     * 
     * @return true 成功，false 失败
     */
    bool processRecentsScreenShow(void);

    const Data &_data;                         /*!< 显示配置数据 */
    AppLauncher _app_launcher;                 /*!< 应用启动器对象 */
    std::shared_ptr<StatusBar> _status_bar;    /*!< 状态栏对象 */
    std::shared_ptr<NavigationBar> _navigation_bar;  /*!< 导航栏对象 */
    std::shared_ptr<RecentsScreen> _recents_screen;  /*!< 最近屏幕对象 */
};

} // namespace esp_brookesia::systems::phone

/**
 * @brief 向后兼容性定义
 */
using ESP_Brookesia_PhoneDisplayData_t [[deprecated("Use `esp_brookesia::systems::phone::Display::Data` instead")]] =
    esp_brookesia::systems::phone::Display::Data;
using ESP_Brookesia_PhoneDisplay [[deprecated("Use `esp_brookesia::systems::phone::Display` instead")]] =
    esp_brookesia::systems::phone::Display;