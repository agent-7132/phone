/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief 样式模块实现文件
 * 
 * 提供样式系统中各结构体的校准方法实现，包括尺寸、字体、图片等属性的校准逻辑。
 * 校准过程负责将用户配置的百分比、自动等模式转换为实际的像素值。
 */

#include "private/esp_brookesia_gui_style_utils.hpp"
#include "esp_brookesia_gui_style.hpp"

namespace esp_brookesia::gui {

/**
 * @brief 根据父容器尺寸校准当前尺寸
 * 
 * 根据父容器的尺寸和当前配置的标志位，计算实际的宽度、高度和圆角半径。
 * 处理百分比模式、自动尺寸、正方形和圆形等特殊情况。
 * 
 * @param parent 父容器的尺寸
 * @return true 校准成功，false 校准失败（如百分比超出范围）
 */
bool StyleSize::calibrate(const StyleSize &parent)
{
    int parent_w = 0;
    int parent_h = 0;

    parent_w = parent.width;
    parent_h = parent.height;

    // Check width
    if (flags.enable_width_auto) {
        width = StyleSize::LENGTH_AUTO;
    } else if (flags.enable_width_percent) {
        ESP_UTILS_CHECK_VALUE_RETURN(width_percent, 1, 100, false, "Invalid width percent");
        width = (parent_w != StyleSize::LENGTH_AUTO) ? (parent_w * width_percent) / 100 : StyleSize::LENGTH_AUTO;
    } else if (width != StyleSize::LENGTH_AUTO) {
        ESP_UTILS_CHECK_VALUE_RETURN(width, 1, parent_w, false, "Invalid width");
    }

    // Check height
    if (flags.enable_height_auto) {
        height = StyleSize::LENGTH_AUTO;
    } else if (flags.enable_height_percent) {
        ESP_UTILS_CHECK_VALUE_RETURN(height_percent, 1, 100, false, "Invalid Height percent");
        height = (parent_h != StyleSize::LENGTH_AUTO) ? (parent_h * height_percent) / 100 : StyleSize::LENGTH_AUTO;
    } else {
        ESP_UTILS_CHECK_VALUE_RETURN(height, 1, parent_h, false, "Invalid Height");
    }

    // Process special size
    if (flags.enable_square || flags.enable_circle) {
        width = std::min(width, height);
        height = width;
    }

    // Process circle
    if (flags.enable_circle) {
        radius = StyleSize::RADIUS_CIRCLE;
    }

    return true;
}

/**
 * @brief 根据父容器尺寸校准当前尺寸（可选检查）
 * 
 * 与calibrate(const StyleSize &)类似，但可选择性地跳过宽度或高度的有效性检查。
 * 适用于某些情况下需要允许尺寸超出父容器范围的场景。
 * 
 * @param parent 父容器的尺寸
 * @param check_width 是否检查宽度有效性（是否超出父容器范围）
 * @param check_height 是否检查高度有效性（是否超出父容器范围）
 * @return true 校准成功，false 校准失败
 */
bool StyleSize::calibrate(const StyleSize &parent, bool check_width, bool check_height)
{
    int parent_w = 0;
    int parent_h = 0;

    parent_w = parent.width;
    parent_h = parent.height;

    // Check width
    if (flags.enable_width_auto) {
        width = StyleSize::LENGTH_AUTO;
    } else if (flags.enable_width_percent) {
        ESP_UTILS_CHECK_VALUE_RETURN(width_percent, 1, 100, false, "Invalid width percent");
        width = (parent_w * width_percent) / 100;
    } else if (check_width) {
        ESP_UTILS_CHECK_VALUE_RETURN(width, 1, parent_w, false, "Invalid width");
    }

    // Check height
    if (flags.enable_height_auto) {
        height = StyleSize::LENGTH_AUTO;
    } else if (flags.enable_height_percent) {
        ESP_UTILS_CHECK_VALUE_RETURN(height_percent, 1, 100, false, "Invalid Height percent");
        height = (parent_h * height_percent) / 100;
    } else if (check_height) {
        ESP_UTILS_CHECK_VALUE_RETURN(height, 1, parent_h, false, "Invalid Height");
    }

    // Process special size
    if (flags.enable_square || flags.enable_circle) {
        width = std::min(width, height);
        height = width;
    }

    // Process circle
    if (flags.enable_circle) {
        radius = StyleSize::RADIUS_CIRCLE;
    }

    return true;
}

/**
 * @brief 根据父容器尺寸校准当前尺寸（允许零值）
 * 
 * 与calibrate(const StyleSize &)类似，但可选择是否允许尺寸为零。
 * 默认情况下，尺寸必须大于0；当allow_zero为true时，允许尺寸为0。
 * 
 * @param parent 父容器的尺寸
 * @param allow_zero 是否允许尺寸为零
 * @return true 校准成功，false 校准失败
 */
bool StyleSize::calibrate(const StyleSize &parent, bool allow_zero)
{
    int parent_w = 0;
    int parent_h = 0;
    int min_size = allow_zero ? 0 : 1;

    parent_w = parent.width;
    parent_h = parent.height;

    // Check width
    if (flags.enable_width_auto) {
        width = StyleSize::LENGTH_AUTO;
    } else if (flags.enable_width_percent) {
        ESP_UTILS_CHECK_VALUE_RETURN(width_percent, min_size, 100, false, "Invalid width percent");
        width = (parent_w * width_percent) / 100;
    } else {
        ESP_UTILS_CHECK_VALUE_RETURN(width, min_size, parent_w, false, "Invalid width");
    }

    // Check height
    if (flags.enable_height_auto) {
        height = StyleSize::LENGTH_AUTO;
    } else if (flags.enable_height_percent) {
        ESP_UTILS_CHECK_VALUE_RETURN(height_percent, min_size, 100, false, "Invalid Height percent");
        height = (parent_h * height_percent) / 100;
    } else {
        ESP_UTILS_CHECK_VALUE_RETURN(height, min_size, parent_h, false, "Invalid Height");
    }

    // Process special size
    if (flags.enable_square || flags.enable_circle) {
        width = std::min(width, height);
        height = width;
    }

    // Process circle
    if (flags.enable_circle) {
        radius = StyleSize::RADIUS_CIRCLE;
    }

    return true;
}

/**
 * @brief 校准字体配置
 * 
 * 根据配置模式（固定大小、高度模式、高度百分比模式）计算实际的字体大小和高度，
 * 并通过回调函数获取字体资源。
 * 
 * 校准流程：
 * 1. 如果启用了高度模式，跳转到高度处理逻辑
 * 2. 否则，验证字体大小在有效范围内（FONT_SIZE_MIN 到 FONT_SIZE_MAX）
 * 3. 如果未指定字体资源，通过回调函数按大小查找字体资源
 * 4. 获取字体行高
 * 
 * 高度模式处理：
 * 1. 如果启用了高度百分比，根据父容器高度计算实际高度
 * 2. 否则，验证高度在有效范围内
 * 3. 通过回调函数按高度查找字体资源，并获取实际字体大小
 * 
 * @param parent 父容器尺寸（用于高度百分比模式）
 * @param find_resource_by_size 按大小查找字体资源的回调函数
 * @param find_resource_by_height 按高度查找字体资源的回调函数
 * @param get_font_line_height 获取字体行高的回调函数
 * @return true 校准成功，false 校准失败
 */
bool StyleFont::calibrate(
    const StyleSize *parent, FindResourceBySizeMethod find_resource_by_size,
    FindResourceByHeightMethod find_resource_by_height, GetFontLineHeightMethod get_font_line_height
)
{
    if (flags.enable_height) {
        goto process_height;
    }

    // Size
    ESP_UTILS_CHECK_VALUE_RETURN(
        size_px, StyleFont::FONT_SIZE_MIN, StyleFont::FONT_SIZE_MAX, false, "Invalid size"
    );
    // Font description
    if (font_resource == nullptr) {
        font_resource = find_resource_by_size(size_px);
        ESP_UTILS_CHECK_NULL_RETURN(font_resource, false, "Get default font failed");
        height = get_font_line_height(font_resource);
    }
    goto end;

process_height:
    // Height
    if (flags.enable_height_percent) {
        ESP_UTILS_CHECK_NULL_RETURN(parent, false, "Invalid parent");
        ESP_UTILS_CHECK_VALUE_RETURN(height_percent, 1, 100, false, "Invalid height percent");
        height = (parent->height * height_percent) / 100;
    } else if (parent != nullptr) {
        ESP_UTILS_CHECK_VALUE_RETURN(height, 1, parent->height, false, "Invalid height");
    }

    // Font description & size
    font_resource = find_resource_by_height(height, &size_px);
    ESP_UTILS_CHECK_NULL_RETURN(font_resource, false, "Get default font failed");

end:
    return true;
}

/**
 * @brief 校准图片配置
 * 
 * 检查图片资源是否有效，确保图片配置正确。
 * 目前仅验证资源指针不为空。
 * 
 * @return true 校准成功（资源有效），false 校准失败（资源无效）
 */
bool StyleImage::calibrate(void) const
{
    ESP_UTILS_CHECK_NULL_RETURN(resource, false, "Invalid resource");

    return true;
}

} // namespace esp_brookesia::gui
