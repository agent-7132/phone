/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include <array>
#include "lvgl.h"
#include "lvgl/esp_brookesia_lv.hpp"
#include "esp_brookesia_base_app.hpp"

namespace esp_brookesia::systems::base {

class Context;

/**
 * @class Display
 * @brief 显示管理类，负责管理系统的显示屏幕和视觉样式
 * 
 * Display类是系统显示层的核心抽象，负责创建和管理主屏幕(main screen)和系统屏幕(system screen)。
 * 主屏幕用于显示应用程序内容，系统屏幕用于显示系统级UI元素如状态栏、导航栏等。
 * 该类还负责字体校准、尺寸校准和样式管理。
 */
class Display {
public:
    friend class Manager;
    friend class Context;

    /**
     * @brief 调试样式的数量，用于容器边框显示
     */
    static constexpr int DEBUG_STYLES_NUM = 6;

    /**
     * @struct Data
     * @brief Display类的配置数据结构
     * 
     * 包含显示相关的所有配置参数，如背景、文本字体、容器样式等。
     */
    struct Data {
        /**
         * @brief 背景配置
         */
        struct {
            gui::StyleColor color;                              /*!< 背景颜色 */
            gui::StyleImage wallpaper_image_resource;           /*!< 壁纸图片资源 */
        } background;

        /**
         * @brief 文本配置
         */
        struct {
            uint8_t default_fonts_num;                         /*!< 默认字体数量 */
            gui::StyleFont default_fonts[gui::StyleFont::FONT_SIZE_NUM]; /*!< 默认字体数组 */
        } text;

        /**
         * @brief 容器样式配置（用于调试显示边框）
         */
        struct {
            struct {
                uint8_t outline_width;                         /*!< 边框宽度 */
                gui::StyleColor outline_color;                  /*!< 边框颜色 */
            } styles[DEBUG_STYLES_NUM];                        /*!< 样式数组 */
        } container;
    };

    /**
     * @brief 删除拷贝构造函数和赋值运算符，防止对象拷贝
     */
    Display(const Display &) = delete;
    Display(Display &&) = delete;
    Display &operator=(const Display &) = delete;
    Display &operator=(Display &&) = delete;

    /**
     * @brief 构造函数
     * 
     * @param core 系统上下文引用
     * @param data Display配置数据引用
     */
    Display(Context &core, const Data &data);

    /**
     * @brief 析构函数
     */
    ~Display();

    /**
     * @brief 显示容器边框（调试用）
     * 
     * @return true 成功，false 失败
     */
    bool showContainerBorder(void);

    /**
     * @brief 隐藏容器边框
     * 
     * @return true 成功，false 失败
     */
    bool hideContainerBorder(void);

    /**
     * @brief 检查Display是否已初始化
     * 
     * @return true 已初始化，false 未初始化
     */
    bool checkCoreInitialized(void) const
    {
        return (_main_screen != nullptr);
    }

    /**
     * @brief 获取主屏幕的LVGL原生句柄
     * 
     * @return lv_obj_t* 主屏幕对象指针
     */
    lv_obj_t *getMainScreen(void) const
    {
        return _main_screen->getNativeHandle();
    }

    /**
     * @brief 获取系统屏幕的LVGL原生句柄
     * 
     * @return lv_obj_t* 系统屏幕对象指针
     */
    lv_obj_t *getSystemScreen(void) const
    {
        return _system_screen->getNativeHandle();
    }

    /**
     * @brief 获取主屏幕容器对象的LVGL原生句柄
     * 
     * @return lv_obj_t* 主屏幕容器对象指针
     */
    lv_obj_t *getMainScreenObject(void) const
    {
        return _main_screen_obj->getNativeHandle();
    }

    /**
     * @brief 获取系统屏幕容器对象的LVGL原生句柄
     * 
     * @return lv_obj_t* 系统屏幕容器对象指针
     */
    lv_obj_t *getSystemScreenObject(void) const
    {
        return _system_screen_obj->getNativeHandle();
    }

    /**
     * @brief 获取主屏幕的智能指针
     * 
     * @return const esp_brookesia::gui::LvObject* 主屏幕对象指针
     */
    const esp_brookesia::gui::LvObject *getMainScreenPtr(void) const
    {
        return _main_screen.get();
    }

    /**
     * @brief 获取系统屏幕的智能指针
     * 
     * @return const esp_brookesia::gui::LvObject* 系统屏幕对象指针
     */
    const esp_brookesia::gui::LvObject *getSystemScreenPtr(void) const
    {
        return _system_screen.get();
    }

    /**
     * @brief 获取主屏幕容器对象的智能指针
     * 
     * @return const esp_brookesia::gui::LvObject* 主屏幕容器对象指针
     */
    const esp_brookesia::gui::LvObject *getMainScreenObjectPtr(void) const
    {
        return _main_screen_obj.get();
    }

    /**
     * @brief 获取系统屏幕容器对象的智能指针
     * 
     * @return const esp_brookesia::gui::LvObject* 系统屏幕容器对象指针
     */
    const esp_brookesia::gui::LvObject *getSystemScreenObjectPtr(void) const
    {
        return _system_screen_obj.get();
    }

    /**
     * @brief 获取核心容器样式（循环获取）
     * 
     * @return lv_style_t* 容器样式指针
     */
    lv_style_t *getCoreContainerStyle(void);

    /**
     * @brief 校准核心对象尺寸
     * 
     * @param parent 父对象尺寸
     * @param target 目标对象尺寸（将被校准）
     * @return true 成功，false 失败
     */
    bool calibrateCoreObjectSize(
        const esp_brookesia::gui::StyleSize &parent, esp_brookesia::gui::StyleSize &target
    ) const;

    /**
     * @brief 校准核心对象尺寸（带宽度/高度检查控制）
     * 
     * @param parent 父对象尺寸
     * @param target 目标对象尺寸（将被校准）
     * @param check_width 是否检查宽度
     * @param check_height 是否检查高度
     * @return true 成功，false 失败
     */
    bool calibrateCoreObjectSize(
        const esp_brookesia::gui::StyleSize &parent, esp_brookesia::gui::StyleSize &target,
        bool check_width, bool check_height
    ) const;

    /**
     * @brief 校准核心对象尺寸（带零值允许控制）
     * 
     * @param parent 父对象尺寸
     * @param target 目标对象尺寸（将被校准）
     * @param allow_zero 是否允许零值
     * @return true 成功，false 失败
     */
    bool calibrateCoreObjectSize(
        const esp_brookesia::gui::StyleSize &parent, esp_brookesia::gui::StyleSize &target, bool allow_zero
    ) const;

    /**
     * @brief 校准核心字体
     * 
     * @param parent 父对象尺寸（可为nullptr）
     * @param target 目标字体（将被校准）
     * @return true 成功，false 失败
     */
    bool calibrateCoreFont(const esp_brookesia::gui::StyleSize *parent, esp_brookesia::gui::StyleFont &target) const;

    /**
     * @brief 校准核心图标图像
     * 
     * @param target 目标图标图像
     * @return true 成功，false 失败
     */
    bool calibrateCoreIconImage(const esp_brookesia::gui::StyleImage &target) const;

protected:
    Context &_system_context;  /*!< 系统上下文引用 */
    const Data &_core_data;    /*!< 配置数据引用 */

private:
    /**
     * @brief 处理应用安装（纯虚函数，由子类实现）
     * 
     * @param app 应用对象指针
     * @return true 成功，false 失败
     */
    virtual bool processAppInstall(App *app) = 0;

    /**
     * @brief 处理应用卸载（纯虚函数，由子类实现）
     * 
     * @param app 应用对象指针
     * @return true 成功，false 失败
     */
    virtual bool processAppUninstall(App *app) = 0;

    /**
     * @brief 处理应用运行（纯虚函数，由子类实现）
     * 
     * @param app 应用对象指针
     * @return true 成功，false 失败
     */
    virtual bool processAppRun(App *app) = 0;

    /**
     * @brief 处理应用恢复（虚函数，默认实现返回true）
     * 
     * @param app 应用对象指针
     * @return true 成功，false 失败
     */
    virtual bool processAppResume(App *app)
    {
        return true;
    }

    /**
     * @brief 处理应用暂停（虚函数，默认实现返回true）
     * 
     * @param app 应用对象指针
     * @return true 成功，false 失败
     */
    virtual bool processAppPause(App *app)
    {
        return true;
    }

    /**
     * @brief 处理应用关闭（虚函数，默认实现返回true）
     * 
     * @param app 应用对象指针
     * @return true 成功，false 失败
     */
    virtual bool processAppClose(App *app)
    {
        return true;
    }

    /**
     * @brief 处理主屏幕加载
     * 
     * @return true 成功，false 失败
     */
    virtual bool processMainScreenLoad(void);

    /**
     * @brief 获取应用的可视区域（虚函数，默认实现返回true）
     * 
     * @param app 应用对象指针
     * @param app_visual_area 应用可视区域输出
     * @return true 成功，false 失败
     */
    virtual bool getAppVisualArea(App *app, lv_area_t &app_visual_area) const
    {
        return true;
    }

    /**
     * @brief 初始化Display
     * 
     * @return true 成功，false 失败
     */
    bool begin(void);

    /**
     * @brief 销毁Display
     * 
     * @return true 成功，false 失败
     */
    bool del(void);

    /**
     * @brief 根据新数据更新显示
     * 
     * @return true 成功，false 失败
     */
    bool updateByNewData(void);

    /**
     * @brief 校准核心数据
     * 
     * @param data 待校准的数据
     * @return true 成功，false 失败
     */
    bool calibrateCoreData(Data &data);

    /**
     * @brief 保存当前LVGL屏幕状态
     */
    void saveLvScreens(void);

    /**
     * @brief 恢复LVGL屏幕状态
     */
    void loadLvScreens(void);

    /**
     * @brief 内部尺寸校准函数
     * 
     * @param target 目标尺寸
     * @return true 成功，false 失败
     */
    bool calibrateStyleSizeInternal(esp_brookesia::gui::StyleSize &target) const;

    /**
     * @brief 根据字号获取字体
     * 
     * @param size_px 字体大小（像素）
     * @return const lv_font_t* 字体指针，失败返回nullptr
     */
    const lv_font_t *getFontBySize(int size) const;

    /**
     * @brief 根据高度获取字体
     * 
     * @param height 字体高度
     * @param size_px 输出字体大小（像素）
     * @return const lv_font_t* 字体指针，失败返回nullptr
     */
    const lv_font_t *getFontByHeight(int height, int *size_px) const;

    lv_obj_t *_lv_main_screen = nullptr;   /*!< 保存的LVGL主屏幕 */
    lv_obj_t *_lv_system_screen = nullptr; /*!< 保存的LVGL系统屏幕 */

    esp_brookesia::gui::LvScreenUniquePtr _main_screen;         /*!< 主屏幕智能指针 */
    esp_brookesia::gui::LvScreenUniquePtr _system_screen;       /*!< 系统屏幕智能指针 */
    esp_brookesia::gui::LvContainerUniquePtr _main_screen_obj;  /*!< 主屏幕容器智能指针 */
    esp_brookesia::gui::LvContainerUniquePtr _system_screen_obj;/*!< 系统屏幕容器智能指针 */

    uint8_t _container_style_index = 0;                         /*!< 当前容器样式索引 */
    std::array<lv_style_t, DEBUG_STYLES_NUM> _container_styles; /*!< 容器样式数组 */
    std::map<uint8_t, const lv_font_t *> _default_size_font_map; /*!< 默认字号到字体的映射 */
    std::map<uint8_t, const lv_font_t *> _default_height_font_map; /*!< 默认高度到字体的映射 */
    std::map<uint8_t, const lv_font_t *> _update_size_font_map; /*!< 更新字号到字体的映射 */
    std::map<uint8_t, const lv_font_t *> _update_height_font_map; /*!< 更新高度到字体的映射 */
};

} // namespace esp_brookesia::systems::base

/**
 * @brief 向后兼容性定义
 */
using ESP_Brookesia_CoreDisplayData_t [[deprecated("Use `esp_brookesia::systems::base::Display::Data` instead")]] =
    esp_brookesia::systems::base::Display::Data;
using ESP_Brookesia_CoreDisplay [[deprecated("Use `esp_brookesia::systems::base::Display` instead")]] =
    esp_brookesia::systems::base::Display;
#define ESP_BROOKESIA_BASE_DISPLAY_DEBUG_STYLES_NUM   esp_brookesia::systems::base::Display::DEBUG_STYLES_NUM