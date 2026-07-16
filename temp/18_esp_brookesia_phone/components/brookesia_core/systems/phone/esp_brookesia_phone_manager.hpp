/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "lvgl.h"
#include "systems/base/esp_brookesia_base_manager.hpp"
#include "systems/phone/widgets/gesture/esp_brookesia_gesture.hpp"
#include "esp_brookesia_phone_display.hpp"
#include "esp_brookesia_phone_app.hpp"

namespace esp_brookesia::systems::phone {

class Phone;

/**
 * @class Manager
 * @brief 手机系统管理器类
 * 
 * Manager类继承自base::Manager，负责管理手机系统的手势导航、屏幕切换和应用生命周期。
 * 它处理手势事件、导航事件、屏幕状态变化，并管理最近屏幕的显示和交互。
 */
class Manager: public base::Manager {
public:
    friend class Phone;

    /**
     * @struct Data
     * @brief 管理器配置数据结构
     * 
     * 包含手势配置、最近屏幕配置和功能启用标志。
     */
    struct Data {
        Gesture::Data gesture;                          /*!< 手势配置数据 */
        uint32_t gesture_mask_indicator_trigger_time_ms; /*!< 手势遮罩指示器触发时间(毫秒) */

        /**
         * @brief 最近屏幕配置
         */
        struct {
            int drag_snapshot_y_step;                    /*!< 快照拖动Y轴步长 */
            int drag_snapshot_y_threshold;               /*!< 快照拖动Y轴阈值 */
            int drag_snapshot_angle_threshold;           /*!< 快照拖动角度阈值 */
            int delete_snapshot_y_threshold;             /*!< 删除快照Y轴阈值 */
        } recents_screen;

        /**
         * @brief 功能启用标志位
         */
        struct {
            uint8_t enable_gesture: 1;                           /*!< 启动手势 */
            uint8_t enable_gesture_navigation_back: 1;           /*!< 启动手势导航返回 */
            uint8_t enable_recents_screen_snapshot_drag: 1;      /*!< 启用最近屏幕快照拖动 */
            uint8_t enable_recents_screen_hide_when_no_snapshot: 1; /*!< 无快照时隐藏最近屏幕 */
        } flags;
    };

    /**
     * @enum Screen
     * @brief 屏幕类型枚举
     * 
     * 定义手机系统中不同的屏幕类型。
     */
    enum Screen {
        MAIN = 0,              /*!< 主屏幕(应用启动器) */
        APP,                   /*!< 应用屏幕 */
        RECENTS_SCREEN,        /*!< 最近屏幕 */
        MAX,                   /*!< 屏幕类型最大值 */
    };

    /**
     * @brief 构造函数
     * 
     * @param core_in 核心上下文引用
     * @param display_in 显示管理对象引用
     * @param data_in 管理器配置数据引用
     */
    Manager(base::Context &core_in, Display &display_in, const Data &data_in);

    /**
     * @brief 析构函数
     */
    ~Manager();

    /**
     * @brief 检查初始化状态
     * 
     * @return true 已初始化，false 未初始化
     */
    bool checkInitialized(void) const
    {
        return _flags.is_initialized;
    }

    /**
     * @brief 获取手势对象
     * 
     * @return Gesture* 手势对象指针，若未启用则返回nullptr
     */
    Gesture *getGesture(void)
    {
        return _gesture.get();
    }

    /**
     * @brief 校准管理器配置数据
     * 
     * 根据屏幕尺寸和显示配置校准管理器数据。
     * 
     * @param screen_size 屏幕尺寸
     * @param display 显示管理对象引用
     * @param data 待校准的管理器配置数据
     * @return true 成功，false 失败
     */
    static bool calibrateData(const gui::StyleSize &screen_size, Display &display, Data &data);

    Display &display;   /*!< 显示管理对象引用 */
    const Data &data;   /*!< 管理器配置数据引用 */

private:
    /**
     * @brief 处理应用运行额外逻辑
     * 
     * 在应用运行时执行手机系统特有的额外逻辑，如更新最近屏幕快照。
     * 
     * @param app 应用对象指针
     * @return true 成功，false 失败
     */
    bool processAppRunExtra(base::App *app) override;

    /**
     * @brief 处理应用恢复额外逻辑
     * 
     * 在应用恢复时执行手机系统特有的额外逻辑。
     * 
     * @param app 应用对象指针
     * @return true 成功，false 失败
     */
    bool processAppResumeExtra(base::App *app) override;

    /**
     * @brief 处理应用关闭额外逻辑
     * 
     * 在应用关闭时执行手机系统特有的额外逻辑，如清理最近屏幕快照。
     * 
     * @param app 应用对象指针
     * @return true 成功，false 失败
     */
    bool processAppCloseExtra(base::App *app) override;

    /**
     * @brief 处理导航事件
     * 
     * 响应导航请求，如返回、主页、最近应用等。
     * 
     * @param type 导航类型
     * @return true 成功，false 失败
     */
    bool processNavigationEvent(base::Manager::NavigateType type) override;

    /**
     * @brief 初始化管理器
     * 
     * 创建手势对象并注册事件回调。
     * 
     * @return true 成功，false 失败
     */
    bool begin(void);

    /**
     * @brief 销毁管理器
     * 
     * 销毁手势对象并清理资源。
     * 
     * @return true 成功，false 失败
     */
    bool del(void);

    /**
     * @brief 处理状态栏屏幕切换
     * 
     * 根据当前屏幕类型更新状态栏显示模式。
     * 
     * @param screen 当前屏幕类型
     * @param param 附加参数
     * @return true 成功，false 失败
     */
    bool processStatusBarScreenChange(Screen screen, void *param);

    /**
     * @brief 处理导航栏屏幕切换
     * 
     * 根据当前屏幕类型更新导航栏显示模式。
     * 
     * @param screen 当前屏幕类型
     * @param param 附加参数
     * @return true 成功，false 失败
     */
    bool processNavigationBarScreenChange(Screen screen, void *param);

    /**
     * @brief 处理手势屏幕切换
     * 
     * 根据当前屏幕类型更新手势配置。
     * 
     * @param screen 当前屏幕类型
     * @param param 附加参数
     * @return true 成功，false 失败
     */
    bool processGestureScreenChange(Screen screen, void *param);

    /**
     * @brief 处理显示屏幕切换
     * 
     * 统一处理状态栏、导航栏和手势的屏幕切换。
     * 
     * @param screen 当前屏幕类型
     * @param param 附加参数
     * @return true 成功，false 失败
     */
    bool processDisplayScreenChange(Screen screen, void *param);

    /**
     * @brief 主屏幕加载事件回调
     * 
     * 当主屏幕加载完成时触发。
     * 
     * @param event LVGL事件对象
     */
    static void onDisplayMainScreenLoadEventCallback(lv_event_t *event);

    /**
     * @brief 应用启动器手势事件回调
     * 
     * 处理应用启动器上的手势事件。
     * 
     * @param event LVGL事件对象
     */
    static void onAppLauncherGestureEventCallback(lv_event_t *event);

    /**
     * @brief 导航栏手势事件回调
     * 
     * 处理导航栏上的手势事件。
     * 
     * @param event LVGL事件对象
     */
    static void onNavigationBarGestureEventCallback(lv_event_t *event);

    /**
     * @brief 手势导航按下事件回调
     * 
     * 处理手势导航按下操作。
     * 
     * @param event LVGL事件对象
     */
    static void onGestureNavigationPressingEventCallback(lv_event_t *event);

    /**
     * @brief 手势导航释放事件回调
     * 
     * 处理手势导航释放操作。
     * 
     * @param event LVGL事件对象
     */
    static void onGestureNavigationReleaseEventCallback(lv_event_t *event);

    /**
     * @brief 手势遮罩指示器按下事件回调
     * 
     * 处理手势遮罩指示器按下操作。
     * 
     * @param event LVGL事件对象
     */
    static void onGestureMaskIndicatorPressingEventCallback(lv_event_t *event);

    /**
     * @brief 手势遮罩指示器释放事件回调
     * 
     * 处理手势遮罩指示器释放操作。
     * 
     * @param event LVGL事件对象
     */
    static void onGestureMaskIndicatorReleaseEventCallback(lv_event_t *event);

    /**
     * @brief 显示最近屏幕
     * 
     * 切换到最近屏幕并更新相关UI状态。
     * 
     * @return true 成功，false 失败
     */
    bool processRecentsScreenShow(void);

    /**
     * @brief 隐藏最近屏幕
     * 
     * 从最近屏幕切换回之前的屏幕。
     * 
     * @return true 成功，false 失败
     */
    bool processRecentsScreenHide(void);

    /**
     * @brief 处理最近屏幕左移
     * 
     * 在最近屏幕中向左滑动切换到上一个应用。
     * 
     * @return true 成功，false 失败
     */
    bool processRecentsScreenMoveLeft(void);

    /**
     * @brief 处理最近屏幕右移
     * 
     * 在最近屏幕中向右滑动切换到下一个应用。
     * 
     * @return true 成功，false 失败
     */
    bool processRecentsScreenMoveRight(void);

    /**
     * @brief 最近屏幕手势按下事件回调
     * 
     * 处理最近屏幕上的手势按下操作。
     * 
     * @param event LVGL事件对象
     */
    static void onRecentsScreenGesturePressEventCallback(lv_event_t *event);

    /**
     * @brief 最近屏幕手势按压事件回调
     * 
     * 处理最近屏幕上的手势按压操作。
     * 
     * @param event LVGL事件对象
     */
    static void onRecentsScreenGesturePressingEventCallback(lv_event_t *event);

    /**
     * @brief 最近屏幕手势释放事件回调
     * 
     * 处理最近屏幕上的手势释放操作。
     * 
     * @param event LVGL事件对象
     */
    static void onRecentsScreenGestureReleaseEventCallback(lv_event_t *event);

    /**
     * @brief 最近屏幕快照删除事件回调
     * 
     * 当最近屏幕快照被删除时触发。
     * 
     * @param event LVGL事件对象
     */
    static void onRecentsScreenSnapshotDeletedEventCallback(lv_event_t *event);

    /**
     * @brief 内部状态标志位
     */
    struct {
        uint8_t is_initialized: 1;                       /*!< 是否已初始化 */
        uint8_t is_app_launcher_gesture_disabled: 1;     /*!< 应用启动器手势是否禁用 */
        uint8_t enable_navigation_bar_gesture: 1;        /*!< 是否启用导航栏手势 */
        uint8_t is_navigation_bar_gesture_disabled: 1;   /*!< 导航栏手势是否禁用 */
        uint8_t enable_gesture_navigation: 1;            /*!< 是否启动手势导航 */
        uint8_t enable_gesture_navigation_back: 1;       /*!< 是否启动手势导航返回 */
        uint8_t enable_gesture_navigation_home: 1;       /*!< 是否启动手势导航主页 */
        uint8_t enable_gesture_navigation_recents_app: 1; /*!< 是否启动手势导航最近应用 */
        uint8_t is_gesture_navigation_disabled: 1;       /*!< 手势导航是否禁用 */
        uint8_t enable_gesture_show_mask_left_right_edge: 1; /*!< 是否显示左右边缘手势遮罩 */
        uint8_t enable_gesture_show_mask_bottom_edge: 1;  /*!< 是否显示底部边缘手势遮罩 */
        uint8_t enable_gesture_show_left_right_indicator_bar: 1; /*!< 是否显示左右边缘指示条 */
        uint8_t enable_gesture_show_bottom_indicator_bar: 1; /*!< 是否显示底部边缘指示条 */
        uint8_t is_recents_screen_pressed: 1;            /*!< 最近屏幕是否被按下 */
        uint8_t is_recents_screen_snapshot_move_hor: 1;  /*!< 最近屏幕快照是否正在水平移动 */
        uint8_t is_recents_screen_snapshot_move_ver: 1;  /*!< 最近屏幕快照是否正在垂直移动 */
    } _flags = {};

    Screen _display_active_screen = Screen::MAX;         /*!< 当前活动屏幕类型 */
    Gesture::Direction _app_launcher_gesture_dir = Gesture::DIR_NONE; /*!< 应用启动器手势方向 */
    Gesture::Direction _navigation_bar_gesture_dir = Gesture::DIR_NONE; /*!< 导航栏手势方向 */
    std::unique_ptr<Gesture> _gesture;                   /*!< 手势对象 */
    float _recents_screen_drag_tan_threshold = 0;        /*!< 最近屏幕拖动角度正切阈值 */
    lv_point_t _recents_screen_start_point = {};         /*!< 最近屏幕拖动起始点 */
    lv_point_t _recents_screen_last_point = {};          /*!< 最近屏幕拖动最后点 */
    base::App *_recents_screen_active_app = nullptr;      /*!< 最近屏幕当前活动应用 */
    base::App *_recents_screen_pause_app = nullptr;      /*!< 最近屏幕暂停的应用 */
};

} // namespace esp_brookesia::systems::phone

/**
 * @brief 向后兼容性定义
 */
using ESP_Brookesia_PhoneManagerData_t [[deprecated("Use `esp_brookesia::systems::phone::Manager::Data` instead")]] =
    esp_brookesia::systems::phone::Manager::Data;
using ESP_Brookesia_PhoneManager [[deprecated("Use `esp_brookesia::systems::phone::Manager` instead")]] =
    esp_brookesia::systems::phone::Manager;
using ESP_Brookesia_PhoneManagerScreen_t [[deprecated("Use `esp_brookesia::systems::phone::Manager::Screen` instead")]] =
    esp_brookesia::systems::phone::Manager::Screen;
#define ESP_BROOKESIA_PHONE_MANAGER_SCREEN_MAIN            ESP_Brookesia_PhoneManagerScreen_t::MAIN
#define ESP_BROOKESIA_PHONE_MANAGER_SCREEN_APP             ESP_Brookesia_PhoneManagerScreen_t::APP
#define ESP_BROOKESIA_PHONE_MANAGER_SCREEN_RECENTS_SCREEN  ESP_Brookesia_PhoneManagerScreen_t::RECENTS_SCREEN
#define ESP_BROOKESIA_PHONE_MANAGER_SCREEN_MAX             ESP_Brookesia_PhoneManagerScreen_t::MAX