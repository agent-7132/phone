/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief 样式表管理器头文件
 * 
 * 提供样式表的管理功能，包括样式表的添加、激活、查询等操作。
 * 支持按屏幕分辨率和名称组织样式表，实现多分辨率适配。
 * 
 * @note 该文件包含模板类定义，可用于管理任意类型的样式表结构。
 */

#pragma once

#include <memory>
#include <string>
#include <list>
#include <map>
#include <unordered_map>
#include "style/esp_brookesia_gui_style.hpp"

namespace esp_brookesia::gui {

/**
 * @brief 名称-样式表映射类型
 * 
 * 使用无序映射存储样式表名称到样式表对象的映射。
 * 键为样式表名称（字符串），值为样式表对象的共享指针。
 */
template <typename T>
using NameStylesheetMap = std::unordered_map<std::string, std::shared_ptr<T>>;

/**
 * @brief 分辨率-名称-样式表映射类型
 * 
 * 使用有序映射存储屏幕分辨率到名称-样式表映射的映射。
 * 键为分辨率值（高16位为宽度，低16位为高度），值为名称-样式表映射。
 */
template <typename T>
using ResolutionNameStylesheetMap = std::map<uint32_t, NameStylesheetMap<T>>;

// *INDENT-OFF*
/**
 * @brief 样式表管理器模板类
 * 
 * StylesheetManager是一个模板类，用于管理应用程序中的样式表。
 * 它支持按屏幕分辨率和名称组织样式表，实现多分辨率适配。
 * 子类需要实现纯虚函数calibrateScreenSize和calibrateStylesheet来提供具体的校准逻辑。
 * 
 * @tparam T 样式表类型，通常是包含各种样式属性的结构体
 */
template <typename T>
class StylesheetManager {
public:
    /**
     * @brief 构造函数
     * 
     * 初始化样式表管理器，清空所有内部数据结构。
     */
    StylesheetManager();

    /**
     * @brief 析构函数
     * 
     * 清理所有已注册的样式表资源。
     */
    virtual ~StylesheetManager();

    /**
     * @brief 校准屏幕尺寸（纯虚函数）
     * 
     * 子类必须实现此方法，用于校验和校准屏幕尺寸参数。
     * 
     * @param size 待校准的屏幕尺寸
     * @return true 校准成功，false 校准失败
     */
    virtual bool calibrateScreenSize(StyleSize &size) = 0;

    /**
     * @brief 添加样式表
     * 
     * 将指定名称和屏幕尺寸的样式表添加到管理器中。
     * 样式表会先经过校准处理，然后存储到内部映射结构中。
     * 
     * @param name 样式表名称
     * @param screen_size 屏幕尺寸
     * @param stylesheet 样式表内容
     * @return true 添加成功，false 添加失败
     */
    bool addStylesheet(const char *name, const StyleSize &screen_size, const T &stylesheet);

    /**
     * @brief 激活样式表（直接使用样式表对象）
     * 
     * 将指定的样式表对象设置为当前活动样式表。
     * 样式表会先经过校准处理。
     * 
     * @param screen_size 屏幕尺寸
     * @param stylesheet 样式表内容
     * @return true 激活成功，false 激活失败
     */
    bool activateStylesheet(const StyleSize &screen_size, const T &stylesheet);

    /**
     * @brief 激活样式表（按名称和屏幕尺寸）
     * 
     * 根据名称和屏幕尺寸查找并激活样式表。
     * 
     * @param name 样式表名称
     * @param screen_size 屏幕尺寸
     * @return true 激活成功，false 激活失败（如样式表不存在）
     */
    bool activateStylesheet(const char *name, const StyleSize &screen_size);

    /**
     * @brief 获取已注册样式表的总数
     * 
     * 遍历所有分辨率和名称，统计已注册的样式表数量。
     * 
     * @return size_t 样式表总数
     */
    size_t getStylesheetCount(void) const;

    /**
     * @brief 查找指定屏幕尺寸对应的名称-样式表映射
     * 
     * 根据屏幕尺寸计算分辨率，然后查找对应的名称-样式表映射。
     * 
     * @param screen_size 屏幕尺寸
     * @return NameStylesheetMap<T>::iterator 映射的迭代器，如果未找到则返回end()
     */
    typename NameStylesheetMap<T>::iterator findNameStylesheetMap(const StyleSize &screen_size);

    /**
     * @brief 获取指定屏幕尺寸对应的名称-样式表映射的结束迭代器
     * 
     * 根据屏幕尺寸计算分辨率，然后获取对应的名称-样式表映射的结束迭代器。
     * 
     * @param screen_size 屏幕尺寸
     * @return NameStylesheetMap<T>::iterator 映射的结束迭代器
     */
    typename NameStylesheetMap<T>::iterator getNameStylesheetMapEnd(const StyleSize &screen_size);

    /**
     * @brief 获取当前活动样式表
     * 
     * 返回当前已激活的样式表指针。
     * 
     * @return const T* 当前活动样式表指针
     */
    const T *getStylesheet(void) const { return &_active_stylesheet; }

    /**
     * @brief 根据名称和屏幕尺寸获取样式表
     * 
     * 根据指定的名称和屏幕尺寸查找样式表。
     * 
     * @param name 样式表名称
     * @param screen_size 屏幕尺寸
     * @return const T* 找到的样式表指针，未找到返回nullptr
     */
    const T *getStylesheet(const char *name, const StyleSize &screen_size);

    /**
     * @brief 获取指定屏幕尺寸的第一个样式表
     * 
     * 根据屏幕尺寸查找对应的样式表，返回第一个找到的样式表。
     * 
     * @param screen_size 屏幕尺寸
     * @return const T* 找到的样式表指针，未找到返回nullptr
     */
    const T *getStylesheet(const StyleSize &screen_size);

protected:
    T _active_stylesheet;  /*!< 当前活动样式表 */

    /**
     * @brief 校准样式表（纯虚函数）
     * 
     * 子类必须实现此方法，用于根据屏幕尺寸校准样式表中的各项属性。
     * 
     * @param screen_size 屏幕尺寸
     * @param stylesheet 待校准的样式表
     * @return true 校准成功，false 校准失败
     */
    virtual bool calibrateStylesheet(const StyleSize &screen_size, T &stylesheet) = 0;

    /**
     * @brief 删除所有样式表
     * 
     * 清空当前活动样式表和所有已注册的样式表。
     * 
     * @return true 删除成功
     */
    bool del(void);

private:
    ResolutionNameStylesheetMap<T> _resolution_name_stylesheet_map;  /*!< 分辨率-名称-样式表映射 */

    /**
     * @brief 将屏幕尺寸转换为分辨率值
     * 
     * 将宽度和高度编码为一个32位整数，高16位存储宽度，低16位存储高度。
     * 
     * @param screen_size 屏幕尺寸
     * @return uint32_t 分辨率值
     */
    uint32_t getResolution(const StyleSize &screen_size)
    {
        return (screen_size.width << 16) | screen_size.height;
    }
};
// *INDENT-ON*

template <typename T>
StylesheetManager<T>::StylesheetManager():
    _active_stylesheet{}
{
}

template <typename T>
StylesheetManager<T>::~StylesheetManager()
{
    // ESP_UTILS_LOGD("Delete(0x%p)", this);
    if (!del()) {
        // ESP_UTILS_LOGE("Failed to delete");
    }
}

template <typename T>
bool StylesheetManager<T>::addStylesheet(const char *name, const StyleSize &screen_size, const T &stylesheet)
{
    uint32_t resolution = 0;
    StyleSize calibrate_size = screen_size;
    std::shared_ptr<T> calibration_stylesheet = std::make_shared<T>(stylesheet);

    // ESP_UTILS_CHECK_NULL_RETURN(name, false, "Invalid name");
    // ESP_UTILS_CHECK_NULL_RETURN(calibration_stylesheet, false, "Create stylesheet failed");
    if (name == nullptr || calibration_stylesheet == nullptr) {
        return false;
    }

    // ESP_UTILS_CHECK_FALSE_RETURN(calibrateScreenSize(calibrate_size), false, "Invalid screen size");
    // ESP_UTILS_LOGD("Add stylesheet(%s - %dx%d)", name, calibrate_size.width, calibrate_size.height);
    if (!calibrateScreenSize(calibrate_size)) {
        return false;
    }

    // ESP_UTILS_CHECK_FALSE_RETURN(calibrateStylesheet(calibrate_size, *calibration_stylesheet), false, "Invalid stylesheet");
    if (!calibrateStylesheet(calibrate_size, *calibration_stylesheet)) {
        return false;
    }

    // Check if the resolution is already exist
    resolution = getResolution(calibrate_size);
    auto it_resolution_map = _resolution_name_stylesheet_map.find(resolution);
    // If not exist, create a new map which contains the name and data
    if (it_resolution_map == _resolution_name_stylesheet_map.end()) {
        _resolution_name_stylesheet_map[resolution][std::string(name)] = calibration_stylesheet;
        return true;
    }

    // If exist, check if the name is already exist
    auto it_name_map = it_resolution_map->second.find(name);
    // If exist, overwrite it, else add it
    if (it_name_map != it_resolution_map->second.end()) {
        // ESP_UTILS_LOGW("Stylesheet(%s) already exist, overwrite it", it_name_map->first.c_str());
        it_name_map->second = calibration_stylesheet;
    } else {
        it_resolution_map->second[name] = calibration_stylesheet;
    }

    return true;
}

template <typename T>
bool StylesheetManager<T>::activateStylesheet(const StyleSize &screen_size,
        const T &stylesheet)
{
    StyleSize calibrate_size = screen_size;
    // ESP_UTILS_CHECK_FALSE_RETURN(calibrateScreenSize(calibrate_size), false, "Invalid screen size");
    if (!calibrateScreenSize(calibrate_size)) {
        return false;
    }
    // ESP_UTILS_LOGD("Activate stylesheet(%dx%d)", calibrate_size.width, calibrate_size.height);

    std::shared_ptr<T> calibration_stylesheet = std::make_shared<T>(stylesheet);
    // ESP_UTILS_CHECK_NULL_RETURN(calibration_stylesheet, false, "Create stylesheet failed");
    if (calibration_stylesheet == nullptr) {
        return false;
    }
    // ESP_UTILS_CHECK_FALSE_RETURN(
    //     calibrateStylesheet(calibrate_size, *calibration_stylesheet), false, "Invalid stylesheet"
    // );
    if (!calibrateStylesheet(calibrate_size, *calibration_stylesheet)) {
        return false;
    }

    _active_stylesheet = *calibration_stylesheet;

    return true;
}

template <typename T>
bool StylesheetManager<T>::activateStylesheet(const char *name, const StyleSize &screen_size)
{
    StyleSize calibrate_size = screen_size;
    const T *stylesheet = nullptr;

    // ESP_UTILS_CHECK_NULL_RETURN(name, false, "Invalid name");
    if (name == nullptr) {
        return false;
    }

    // ESP_UTILS_CHECK_FALSE_RETURN(calibrateScreenSize(calibrate_size), false, "Invalid screen size");
    if (!calibrateScreenSize(calibrate_size)) {
        return false;
    }
    // ESP_UTILS_LOGD("Activate stylesheet(%s - %dx%d)", name, calibrate_size.width, calibrate_size.height);

    stylesheet = getStylesheet(name, screen_size);
    // ESP_UTILS_CHECK_NULL_RETURN(stylesheet, false, "Get stylesheet failed");
    if (stylesheet == nullptr) {
        return false;
    }

    _active_stylesheet = *stylesheet;

    return true;
}

template <typename T>
size_t StylesheetManager<T>::getStylesheetCount(void) const
{
    size_t count = 0;

    for (auto &name_stylesheet_map : _resolution_name_stylesheet_map) {
        count += name_stylesheet_map.second.size();
    }

    return count;
}

template <typename T>
typename NameStylesheetMap<T>::iterator StylesheetManager<T>::findNameStylesheetMap(const StyleSize &screen_size)
{
    uint32_t resolution = 0;
    StyleSize calibrate_size = screen_size;

    // ESP_UTILS_CHECK_FALSE_RETURN(calibrateScreenSize(calibrate_size), false, "Invalid screen size");
    if (!calibrateScreenSize(calibrate_size)) {
        return false;
    }
    resolution = getResolution(calibrate_size);

    return _resolution_name_stylesheet_map.find(resolution);
}

template <typename T>
typename NameStylesheetMap<T>::iterator StylesheetManager<T>::getNameStylesheetMapEnd(const StyleSize &screen_size)
{
    uint32_t resolution = 0;
    StyleSize calibrate_size = screen_size;

    // ESP_UTILS_CHECK_FALSE_RETURN(calibrateScreenSize(calibrate_size), false, "Invalid screen size");
    if (!calibrateScreenSize(calibrate_size)) {
        return false;
    }
    resolution = getResolution(calibrate_size);

    return _resolution_name_stylesheet_map[resolution].end();
}

template <typename T>
const T *StylesheetManager<T>::getStylesheet(const char *name, const StyleSize &screen_size)
{
    uint32_t resolution = 0;
    StyleSize calibrate_size = screen_size;

    // ESP_UTILS_CHECK_NULL_RETURN(name, nullptr, "Invalid name");
    if (name == nullptr) {
        return nullptr;
    }

    // ESP_UTILS_CHECK_FALSE_RETURN(calibrateScreenSize(calibrate_size), nullptr, "Invalid screen size");
    if (!calibrateScreenSize(calibrate_size)) {
        return nullptr;
    }

    // Check if the resolution is already exist
    resolution = getResolution(calibrate_size);
    auto it_resolution_map = _resolution_name_stylesheet_map.find(resolution);
    if (it_resolution_map == _resolution_name_stylesheet_map.end()) {
        return nullptr;
    }

    // If exist, check if the name is already exist
    auto it_name_map = it_resolution_map->second.find(name);
    if (it_name_map == it_resolution_map->second.end()) {
        return nullptr;
    }

    return it_name_map->second.get();
}

template <typename T>
const T *StylesheetManager<T>::getStylesheet(const StyleSize &screen_size)
{
    uint32_t resolution = 0;
    StyleSize calibrate_size = screen_size;

    // ESP_UTILS_CHECK_FALSE_RETURN(calibrateScreenSize(calibrate_size), nullptr, "Invalid screen size");
    if (!calibrateScreenSize(calibrate_size)) {
        return nullptr;
    }
    // ESP_UTILS_LOGD("Get stylesheet with resolution(%dx%d)", calibrate_size.width, calibrate_size.height);

    // Check if the resolution is already exist
    resolution = getResolution(calibrate_size);
    auto it_resolution_map = _resolution_name_stylesheet_map.find(resolution);
    if (it_resolution_map == _resolution_name_stylesheet_map.end()) {
        return nullptr;
    }

    auto name_map = it_resolution_map->second;
    if (name_map.empty()) {
        return nullptr;
    }

    return name_map.begin()->second.get();
}

template <typename T>
bool StylesheetManager<T>::del(void)
{
    _active_stylesheet = {};
    _resolution_name_stylesheet_map.clear();

    return true;
}

} // namespace esp_brookesia::gui

template <typename T>
using ESP_Brookesia_NameStylesheetMap [[deprecated("Use `esp_brookesia::gui::NameStylesheetMap` instead")]] =
    esp_brookesia::gui::NameStylesheetMap<T>;
template <typename T>
using ESP_Brookesia_ResolutionNameStylesheetMap [[deprecated("Use `esp_brookesia::gui::ResolutionNameStylesheetMap` instead")]] =
    esp_brookesia::gui::ResolutionNameStylesheetMap<T>;
template <typename T>
using ESP_Brookesia_CoreStylesheetManager [[deprecated("Use `esp_brookesia::gui::StylesheetManager` instead")]] =
    esp_brookesia::gui::StylesheetManager<T>;
