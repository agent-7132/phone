/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <functional>
#include <limits>
#include <stdint.h>

namespace esp_brookesia::gui {

/**
 * @brief 样式宽度项枚举
 * 
 * 定义UI元素的宽度相关属性类型，用于样式系统中指定需要设置的宽度属性。
 */
enum StyleWidthItem {
    STYLE_WIDTH_ITEM_BORDER = 0,        /*!< UI元素的边框宽度 */
    STYLE_WIDTH_ITEM_OUTLINE,           /*!< UI元素的轮廓宽度 */
    STYLE_WIDTH_ITEM_MAX,               /*!< 宽度项类型的最大值，用于数组大小定义 */
};

/**
 * @brief 样式尺寸结构体
 * 
 * StyleSize结构体用于定义UI元素的尺寸属性，包括宽度、高度、圆角半径等。
 * 支持多种尺寸模式：固定像素值、百分比、自动尺寸、正方形、圆形等。
 * 用户可通过静态工厂方法（如RECT、SQUARE、CIRCLE等）便捷地创建尺寸配置。
 */
struct StyleSize {
    static constexpr int LENGTH_AUTO = std::numeric_limits<int>::max();  /*!< 自动长度标记值，表示尺寸由内容或父容器决定 */
    static constexpr int RADIUS_CIRCLE = std::numeric_limits<int>::max(); /*!< 圆形半径标记值，表示圆角为圆形 */

    /**
     * @brief 创建矩形尺寸配置（固定像素值）
     * 
     * 使用指定的宽度和高度像素值创建StyleSize对象。
     * 
     * @param w 宽度（像素）
     * @param h 高度（像素）
     * @return StyleSize 配置好的尺寸对象
     */
    static constexpr StyleSize RECT(int w, int h)
    {
        return {
            .width = w,
            .height = h,
        };
    }

    /**
     * @brief 创建矩形尺寸配置（百分比模式）
     * 
     * 使用父容器尺寸的百分比来创建StyleSize对象。宽度和高度将根据父容器尺寸动态计算。
     * 
     * @param w_percent 宽度占父容器宽度的百分比（1-100）
     * @param h_percent 高度占父容器高度的百分比（1-100）
     * @return StyleSize 配置好的尺寸对象
     */
    static constexpr StyleSize RECT_PERCENT(int w_percent, int h_percent)
    {
        return {
            .width_percent = w_percent,
            .height_percent = h_percent,
            .flags = {
                .enable_width_percent = true,
                .enable_height_percent = true,
            },
        };
    }

    /**
     * @brief 创建矩形尺寸配置（宽度百分比，高度固定）
     * 
     * 宽度使用父容器宽度的百分比，高度使用固定像素值。
     * 
     * @param w_percent 宽度占父容器宽度的百分比（1-100）
     * @param h 高度（像素）
     * @return StyleSize 配置好的尺寸对象
     */
    static constexpr StyleSize RECT_W_PERCENT(int w_percent, int h)
    {
        return {
            .height = h,
            .width_percent = w_percent,
            .flags = {
                .enable_width_percent = true,
            },
        };
    }

    /**
     * @brief 创建矩形尺寸配置（宽度固定，高度百分比）
     * 
     * 宽度使用固定像素值，高度使用父容器高度的百分比。
     * 
     * @param w 宽度（像素）
     * @param h_percent 高度占父容器高度的百分比（1-100）
     * @return StyleSize 配置好的尺寸对象
     */
    static constexpr StyleSize RECT_H_PERCENT(int w, int h_percent)
    {
        return {
            .width = w,
            .height_percent = h_percent,
            .flags = {
                .enable_height_percent = true,
            },
        };
    }

    /**
     * @brief 创建正方形尺寸配置（固定像素值）
     * 
     * 创建宽度和高度相等的正方形尺寸配置。
     * 
     * @param size 边长（像素）
     * @return StyleSize 配置好的尺寸对象
     */
    static constexpr StyleSize SQUARE(int size)
    {
        return {
            .width = size,
            .height = size,
        };
    }

    /**
     * @brief 创建正方形尺寸配置（百分比模式）
     * 
     * 创建宽度和高度相等的正方形尺寸配置，边长为父容器尺寸的百分比。
     * 
     * @param percent 边长占父容器尺寸的百分比（1-100）
     * @return StyleSize 配置好的尺寸对象
     */
    static constexpr StyleSize SQUARE_PERCENT(int percent)
    {
        return {
            .width_percent = percent,
            .height_percent = percent,
            .flags = {
                .enable_width_percent = true,
                .enable_height_percent = true,
                .enable_square = true,
            },
        };
    }

    /**
     * @brief 创建圆形尺寸配置（固定像素值）
     * 
     * 创建圆形尺寸配置，宽度和高度相等，圆角设置为圆形。
     * 
     * @param size 直径（像素）
     * @return StyleSize 配置好的尺寸对象
     */
    static constexpr StyleSize CIRCLE(int size)
    {
        return {
            .width = size,
            .height = size,
            .flags = {
                .enable_circle = true,
            },
        };
    }

    /**
     * @brief 创建圆形尺寸配置（百分比模式）
     * 
     * 创建圆形尺寸配置，直径为父容器尺寸的百分比，圆角设置为圆形。
     * 
     * @param percent 直径占父容器尺寸的百分比（1-100）
     * @return StyleSize 配置好的尺寸对象
     */
    static constexpr StyleSize CIRCLE_PERCENT(int percent)
    {
        return {
            .width_percent = percent,
            .height_percent = percent,
            .flags = {
                .enable_width_percent = true,
                .enable_height_percent = true,
                .enable_circle = true,
            },
        };
    }

    /**
     * @brief 根据父容器尺寸校准当前尺寸
     * 
     * 根据父容器的尺寸和当前配置的标志位，计算实际的宽度、高度和圆角半径。
     * 处理百分比模式、自动尺寸、正方形和圆形等特殊情况。
     * 
     * @param parent 父容器的尺寸
     * @return true 校准成功，false 校准失败（如百分比超出范围）
     */
    bool calibrate(const StyleSize &parent);

    /**
     * @brief 根据父容器尺寸校准当前尺寸（可选检查）
     * 
     * 与calibrate(const StyleSize &)类似，但可选择性地跳过宽度或高度的有效性检查。
     * 
     * @param parent 父容器的尺寸
     * @param check_width 是否检查宽度有效性
     * @param check_height 是否检查高度有效性
     * @return true 校准成功，false 校准失败
     */
    bool calibrate(const StyleSize &parent, bool check_width, bool check_height);

    /**
     * @brief 根据父容器尺寸校准当前尺寸（允许零值）
     * 
     * 与calibrate(const StyleSize &)类似，但可选择是否允许尺寸为零。
     * 
     * @param parent 父容器的尺寸
     * @param allow_zero 是否允许尺寸为零
     * @return true 校准成功，false 校准失败
     */
    bool calibrate(const StyleSize &parent, bool allow_zero);

    int width;                          /*!< 宽度（像素） */
    int height;                         /*!< 高度（像素） */
    int radius;                         /*!< 圆角半径（像素） */
    int width_percent;                  /*!< 宽度占父容器宽度的百分比 */
    int height_percent;                 /*!< 高度占父容器高度的百分比 */
    struct {
        int enable_width_percent: 1;    /*!< 启用宽度百分比模式，宽度将根据width_percent计算 */
        int enable_width_auto: 1;       /*!< 启用自动宽度模式，宽度由内容或父容器决定 */
        int enable_height_percent: 1;   /*!< 启用高度百分比模式，高度将根据height_percent计算 */
        int enable_height_auto: 1;      /*!< 启用自动高度模式，高度由内容或父容器决定 */
        int enable_square: 1;           /*!< 启用正方形模式，宽高取较小值保持相等 */
        int enable_circle: 1;           /*!< 启用圆形模式，宽高取较小值保持相等，半径设为圆形 */
    } flags;                            /*!< 尺寸配置标志位 */
};

/**
 * @brief 样式字体结构体
 * 
 * StyleFont结构体用于定义UI元素的字体属性，包括字体大小、行高、字体资源等。
 * 支持多种字体配置模式：固定像素大小、基于高度、高度百分比等。
 * 提供回调方法用于动态查找字体资源，实现字体的按需加载和匹配。
 */
struct StyleFont {
    static constexpr int FONT_SIZE_MIN = 8;    /*!< 最小字体大小（像素） */
    static constexpr int FONT_SIZE_MAX = 48;   /*!< 最大字体大小（像素） */
    static constexpr int FONT_SIZE_NUM = ((FONT_SIZE_MAX - FONT_SIZE_MIN) / 2 + 1); /*!< 可用字体大小数量 */

    using FindResourceBySizeMethod = std::function<const void *(int size_px)>;          /*!< 按大小查找字体资源的回调类型 */
    using FindResourceByHeightMethod = std::function<const void *(int height, int *size_px)>; /*!< 按高度查找字体资源的回调类型 */
    using GetFontLineHeightMethod = std::function<int (const void *font_resource)>;    /*!< 获取字体行高的回调类型 */

    /**
     * @brief 创建字体配置（固定像素大小）
     * 
     * 使用指定的字体大小创建StyleFont对象，字体资源将在校准时通过回调函数获取。
     * 
     * @param size 字体大小（像素），范围8-48
     * @return StyleFont 配置好的字体对象
     */
    static constexpr StyleFont SIZE(int size)
    {
        return {
            .size_px = size,
        };
    }

    /**
     * @brief 创建字体配置（固定高度）
     * 
     * 使用指定的字体高度创建StyleFont对象，字体大小将在校准时根据高度匹配。
     * 
     * @param h 字体高度（像素）
     * @return StyleFont 配置好的字体对象
     */
    static constexpr StyleFont HEIGHT(int h)
    {
        return {
            .height = h,
            .flags = {
                .enable_height = true,
            },
        };
    }

    /**
     * @brief 创建字体配置（高度百分比模式）
     * 
     * 使用父容器高度的百分比创建StyleFont对象，字体高度将根据父容器尺寸动态计算。
     * 
     * @param percent 字体高度占父容器高度的百分比（1-100）
     * @return StyleFont 配置好的字体对象
     */
    static constexpr StyleFont HEIGHT_PERCENT(int percent)
    {
        return {
            .height_percent = percent,
            .flags = {
                .enable_height = true,
                .enable_height_percent = true,
            },
        };
    }

    /**
     * @brief 创建字体配置（自定义字体资源）
     * 
     * 使用指定的字体大小和自定义字体资源创建StyleFont对象。
     * 
     * @param size 字体大小（像素），范围8-48
     * @param font 自定义字体资源指针
     * @return StyleFont 配置好的字体对象
     */
    static constexpr StyleFont CUSTOM_SIZE(int size, const void *font)
    {
        return {
            .size_px = size,
            .font_resource = font,
        };
    }

    /**
     * @brief 校准字体配置
     * 
     * 根据配置模式（固定大小、高度模式、高度百分比模式）计算实际的字体大小和高度，
     * 并通过回调函数获取字体资源。
     * 
     * @param parent 父容器尺寸（用于高度百分比模式）
     * @param find_resource_by_size 按大小查找字体资源的回调函数
     * @param find_resource_by_height 按高度查找字体资源的回调函数
     * @param get_font_line_height 获取字体行高的回调函数
     * @return true 校准成功，false 校准失败
     */
    bool calibrate(
        const StyleSize *parent, FindResourceBySizeMethod find_resource_by_size,
        FindResourceByHeightMethod find_resource_by_height, GetFontLineHeightMethod get_font_line_height
    );

    int size_px;                        /*!< 字体大小（像素），范围8-48 */
    int height;                         /*!< 字体高度（像素） */
    int height_percent;                 /*!< 字体高度占父容器高度的百分比 */
    const void *font_resource;          /*!< 自定义字体资源指针 */
    struct {
        int enable_height: 1;           /*!< 启用高度模式，字体大小将根据高度计算 */
        int enable_height_percent: 1;   /*!< 启用高度百分比模式，高度将根据height_percent计算 */
    } flags;                            /*!< 字体配置标志位 */
};

/**
 * @brief 字体类型枚举
 * 
 * 定义字体的语言类型，用于区分不同语言的字体资源。
 */
enum StyleFontType {
    TYPE_EN = 0,      /*!< 英文字体 */
    TYPE_CN,          /*!< 中文字体 */
    TYPE_MAX,         /*!< 字体类型的最大值，用于数组大小定义 */
};

/**
 * @brief 样式颜色项枚举
 * 
 * 定义UI元素的颜色属性类型，用于样式系统中指定需要设置的颜色属性。
 */
enum StyleColorItem {
    STYLE_COLOR_ITEM_BACKGROUND = 0,        /*!< UI元素的背景颜色 */
    STYLE_COLOR_ITEM_TEXT,                  /*!< UI元素的文本颜色 */
    STYLE_COLOR_ITEM_BORDER,                /*!< UI元素的边框颜色 */
    STYLE_COLOR_ITEM_MAX,                   /*!< 颜色项类型的最大值，用于数组大小定义 */
};

/**
 * @brief 样式颜色结构体
 * 
 * StyleColor结构体用于定义UI元素的颜色属性，包括RGB颜色值和透明度。
 * 颜色采用24位RGB格式（R[7:0], G[7:0], B[7:0]），透明度采用8位（0-255）。
 */
struct StyleColor {
    /**
     * @brief 创建颜色配置（不透明）
     * 
     * 使用指定的24位RGB颜色值创建StyleColor对象，透明度默认为完全不透明（255）。
     * 
     * @param color24 24位RGB颜色值（格式：R[7:0], G[7:0], B[7:0]）
     * @return StyleColor 配置好的颜色对象
     */
    static constexpr StyleColor COLOR(uint32_t color24)
    {
        return {
            .color = color24,
            .opacity = 255,
        };
    }

    /**
     * @brief 创建颜色配置（带透明度）
     * 
     * 使用指定的24位RGB颜色值和透明度创建StyleColor对象。
     * 
     * @param color24 24位RGB颜色值（格式：R[7:0], G[7:0], B[7:0]）
     * @param opa 透明度值（0-255，0为完全透明，255为完全不透明）
     * @return StyleColor 配置好的颜色对象
     */
    static constexpr StyleColor COLOR_WITH_OPACITY(uint32_t color24, uint8_t opa)
    {
        return {
            .color = color24,
            .opacity = opa,
        };
    }

    uint32_t color: 24;                     /*!< 24位RGB颜色值（R[7:0], G[7:0], B[7:0]） */
    uint32_t opacity: 8;                   /*!< 透明度值（0-255，0为完全透明，255为完全不透明） */
};

/**
 * @brief 样式图片结构体
 * 
 * StyleImage结构体用于定义UI元素的图片资源属性，包括图片资源指针、重着色颜色等。
 * 支持图片重着色功能，可以在渲染时动态改变图片颜色。
 */
struct StyleImage {
    /**
     * @brief 创建图片配置（原始颜色）
     * 
     * 使用指定的图片资源创建StyleImage对象，不进行重着色。
     * 
     * @param image 图片资源指针
     * @return StyleImage 配置好的图片对象
     */
    static constexpr StyleImage IMAGE(const void *image)
    {
        return {
            .resource = image,
        };
    }

    /**
     * @brief 创建图片配置（指定重着色颜色）
     * 
     * 使用指定的图片资源和重着色颜色创建StyleImage对象。
     * 渲染时图片将被重新着色为指定颜色。
     * 
     * @param image 图片资源指针
     * @param color 重着色颜色（24位RGB格式）
     * @return StyleImage 配置好的图片对象
     */
    static constexpr StyleImage IMAGE_RECOLOR(const void *image, uint32_t color)
    {
        return {
            .resource = image,
            .recolor = StyleColor::COLOR(color),
            .flags = {
                .enable_recolor = true,
            },
        };
    }

    /**
     * @brief 创建图片配置（白色重着色）
     * 
     * 使用指定的图片资源创建StyleImage对象，并将图片重着色为白色。
     * 适用于需要将图标统一改为白色的场景。
     * 
     * @param image 图片资源指针
     * @return StyleImage 配置好的图片对象
     */
    static constexpr StyleImage IMAGE_RECOLOR_WHITE(const void *image)
    {
        return {
            .resource = image,
            .recolor = StyleColor::COLOR(0xFFFFFF),
            .flags = {
                .enable_recolor = true,
            },
        };
    }

    /**
     * @brief 创建图片配置（黑色重着色）
     * 
     * 使用指定的图片资源创建StyleImage对象，并将图片重着色为黑色。
     * 适用于需要将图标统一改为黑色的场景。
     * 
     * @param image 图片资源指针
     * @return StyleImage 配置好的图片对象
     */
    static constexpr StyleImage IMAGE_RECOLOR_BLACK(const void *image)
    {
        return {
            .resource = image,
            .recolor = StyleColor::COLOR(0x000000),
            .flags = {
                .enable_recolor = true,
            },
        };
    }

    /**
     * @brief 校准图片配置
     * 
     * 检查图片资源是否有效，确保图片配置正确。
     * 
     * @return true 校准成功，false 图片资源无效
     */
    bool calibrate(void) const;

    const void *resource;                   /*!< 图片资源指针 */
    StyleColor recolor;                     /*!< 图片重着色颜色 */
    StyleColor container_color;             /*!< 容器背景颜色 */
    struct {
        int enable_recolor: 1;              /*!< 启用图片重着色 */
        int enable_container_color: 1;      /*!< 启用容器背景颜色 */
    } flags;                                /*!< 图片配置标志位 */
};

/**
 * @brief 对齐类型枚举
 * 
 * 定义UI元素在容器中的对齐方式，用于定位子元素或内容。
 */
enum StyleAlignType {
    STYLE_ALIGN_TYPE_TOP_LEFT = 0,          /*!< 左上对齐 */
    STYLE_ALIGN_TYPE_TOP_MID,               /*!< 顶部居中对齐 */
    STYLE_ALIGN_TYPE_TOP_RIGHT,             /*!< 右上对齐 */
    STYLE_ALIGN_TYPE_BOTTOM_LEFT,           /*!< 左下对齐 */
    STYLE_ALIGN_TYPE_BOTTOM_MID,            /*!< 底部居中对齐 */
    STYLE_ALIGN_TYPE_BOTTOM_RIGHT,          /*!< 右下对齐 */
    STYLE_ALIGN_TYPE_LEFT_MID,              /*!< 左侧居中对齐 */
    STYLE_ALIGN_TYPE_RIGHT_MID,             /*!< 右侧居中对齐 */
    STYLE_ALIGN_TYPE_CENTER,                /*!< 居中对齐 */
};

/**
 * @brief 样式对齐结构体
 * 
 * StyleAlign结构体用于定义UI元素的对齐方式和偏移量，控制元素在容器中的精确定位。
 */
struct StyleAlign {
    StyleAlignType type;                    /*!< 对齐类型 */
    int offset_x;                           /*!< 水平偏移量（像素） */
    int offset_y;                           /*!< 垂直偏移量（像素） */
};

/**
 * @brief 样式间距结构体
 * 
 * StyleGap结构体用于定义UI元素之间的间距，包括元素四周的外边距、行间距和列间距。
 */
struct StyleGap {
    /**
     * @brief 创建四周间距配置
     * 
     * 设置元素四周（上、下、左、右）的间距。
     * 
     * @param top_v 顶部间距（像素）
     * @param bottom_v 底部间距（像素）
     * @param left_v 左侧间距（像素）
     * @param right_v 右侧间距（像素）
     * @return StyleGap 配置好的间距对象
     */
    static constexpr StyleGap AROUND(int top_v, int bottom_v, int left_v, int right_v)
    {
        return {
            .top = top_v,
            .bottom = bottom_v,
            .left = left_v,
            .right = right_v,
        };
    }

    /**
     * @brief 创建行间距配置
     * 
     * 设置多行布局中每行之间的垂直间距。
     * 
     * @param value 行间距（像素）
     * @return StyleGap 配置好的间距对象
     */
    static constexpr StyleGap ROW(int value)
    {
        return {
            .row = value,
        };
    }

    /**
     * @brief 创建列间距配置
     * 
     * 设置多列布局中每列之间的水平间距。
     * 
     * @param value 列间距（像素）
     * @return StyleGap 配置好的间距对象
     */
    static constexpr StyleGap COLUMN(int value)
    {
        return {
            .column = value,
        };
    }

    int top;                                /*!< 顶部间距（像素） */
    int bottom;                             /*!< 底部间距（像素） */
    int left;                               /*!< 左侧间距（像素） */
    int right;                              /*!< 右侧间距（像素） */
    int row;                                /*!< 行间距（像素） */
    int column;                             /*!< 列间距（像素） */
};

/**
 * @brief 样式布局Flex结构体
 * 
 * StyleLayoutFlex结构体用于定义Flex容器的布局属性，包括排列方向和对齐方式。
 * Flex布局是一种弹性布局模型，可以灵活地排列和对齐子元素。
 */
struct StyleLayoutFlex {
    /**
     * @brief Flex流向类型枚举
     * 
     * 定义Flex容器中元素的排列方向和换行方式。
     */
    enum FlowType {
        FLOW_ROW = 0,                /*!< 元素水平排列 */
        FLOW_COLUMN,                 /*!< 元素垂直排列 */
        FLOW_ROW_WRAP,               /*!< 元素水平排列，超出宽度时换行 */
        FLOW_ROW_REVERSE,            /*!< 元素水平逆向排列 */
        FLOW_ROW_WRAP_REVERSE,       /*!< 元素水平逆向排列，超出宽度时换行 */
        FLOW_COLUMN_WRAP,            /*!< 元素垂直排列，超出高度时换列 */
        FLOW_COLUMN_REVERSE,         /*!< 元素垂直逆向排列 */
        FLOW_COLUMN_WRAP_REVERSE,    /*!< 元素垂直逆向排列，超出高度时换列 */
    };

    /**
     * @brief Flex对齐类型枚举
     * 
     * 定义Flex容器中元素的对齐方式，控制元素在主轴和交叉轴上的位置。
     */
    enum AlignType {
        ALIGN_START,                 /*!< 元素对齐到容器起始位置 */
        ALIGN_END,                   /*!< 元素对齐到容器结束位置 */
        ALIGN_CENTER,                /*!< 元素居中对齐 */
        ALIGN_SPACE_EVENLY,          /*!< 元素均匀分布，间距相等 */
        ALIGN_SPACE_AROUND,          /*!< 元素分布，两侧间距为中间间距的一半 */
        ALIGN_SPACE_BETWEEN,         /*!< 元素分布，首元素和末元素贴边 */
    };

    FlowType flow;                     /*!< Flex元素的排列方向 */
    AlignType main_place;              /*!< 主轴方向的对齐方式 */
    AlignType cross_place;             /*!< 交叉轴方向的对齐方式 */
    AlignType track_place;             /*!< 多行布局时轨道的对齐方式 */
};

/**
 * @brief 样式动画结构体
 * 
 * StyleAnimation结构体用于定义UI元素的动画属性，包括起始值、结束值、持续时间和动画曲线。
 */
struct StyleAnimation {
    /**
     * @brief 动画路径类型枚举
     * 
     * 定义动画的时间函数，控制动画的速度变化曲线。
     */
    enum AnimationPathType {
        ANIM_PATH_TYPE_LINEAR = 0,              /*!< 线性动画，匀速运动 */
        ANIM_PATH_TYPE_EASE_IN,                 /*!< 缓入动画，开始慢，逐渐加速 */
        ANIM_PATH_TYPE_EASE_OUT,                /*!< 缓出动画，开始快，逐渐减速 */
        ANIM_PATH_TYPE_EASE_IN_OUT,             /*!< 缓入缓出动画，开始和结束慢，中间加速 */
        ANIM_PATH_TYPE_OVERSHOOT,               /*!< 过冲动画，超出目标后回弹稳定 */
        ANIM_PATH_TYPE_BOUNCE,                  /*!< 弹跳动画，结束时产生弹跳效果 */
        ANIM_PATH_TYPE_STEP,                    /*!< 步进动画，以离散步骤进行 */
        ANIM_PATH_TYPE_MAX,                     /*!< 动画路径类型的最大值，用于数组大小定义 */
    };

    int start_value;              /*!< 动画起始值 */
    int end_value;                /*!< 动画结束值 */
    int duration_ms;              /*!< 动画持续时间（毫秒） */
    int delay_ms;                 /*!< 动画延迟时间（毫秒） */
    AnimationPathType path_type;  /*!< 动画路径类型（时间函数） */
};

/**
 * @brief 样式标志位枚举
 * 
 * 定义UI元素的行为和显示标志，用于控制元素的各种特性。
 */
enum StyleFlag {
    STYLE_FLAG_HIDDEN                  = (1ULL << 0),  /*!< 元素隐藏 */
    STYLE_FLAG_CLICKABLE               = (1ULL << 1),  /*!< 元素可点击 */
    STYLE_FLAG_SCROLLABLE              = (1ULL << 2),  /*!< 元素可滚动 */
    STYLE_FLAG_EVENT_BUBBLE            = (1ULL << 3),  /*!< 启用事件冒泡 */
    STYLE_FLAG_CLIP_CORNER             = (1ULL << 4),  /*!< 启用圆角裁剪 */
    STYLE_FLAG_SEND_DRAW_TASK_EVENTS   = (1ULL << 5),  /*!< 发送绘制任务事件 */
};

/**
 * @brief 样式标志位按位或运算符重载
 * 
 * 支持两个StyleFlag标志位的按位或运算，用于组合多个标志位。
 * 
 * @param a 第一个样式标志位
 * @param b 第二个样式标志位
 * @return StyleFlag 组合后的样式标志位
 */
inline StyleFlag operator|(StyleFlag a, StyleFlag b)
{
    return static_cast<StyleFlag>((static_cast<int>(a) | static_cast<int>(b)));
}

} // namespace esp_brookesia::gui

/**
 * @brief 宏定义向后兼容层
 * 
 * 提供旧版API的宏定义，确保现有代码在升级到新版本后仍能正常编译。
 * 建议新代码直接使用命名空间中的类型和方法。
 */
#define ESP_BROOKESIA_STYLE_SIZE_RECT(w, h)                 esp_brookesia::gui::StyleSize::RECT(w, h)
#define ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(w_p, h_p)     esp_brookesia::gui::StyleSize::RECT_PERCENT(w_p, h_p)
#define ESP_BROOKESIA_STYLE_SIZE_RECT_W_PERCENT(w_p, h)     esp_brookesia::gui::StyleSize::RECT_W_PERCENT(w_p, h)
#define ESP_BROOKESIA_STYLE_SIZE_RECT_H_PERCENT(w, h_p)     esp_brookesia::gui::StyleSize::RECT_H_PERCENT(w, h_p)
#define ESP_BROOKESIA_STYLE_SIZE_SQUARE(s)                  esp_brookesia::gui::StyleSize::SQUARE(s)
#define ESP_BROOKESIA_STYLE_SIZE_SQUARE_PERCENT(p)          esp_brookesia::gui::StyleSize::SQUARE_PERCENT(p)
#define ESP_BROOKESIA_STYLE_SIZE_CIRCLE(s)                  esp_brookesia::gui::StyleSize::CIRCLE(s)
#define ESP_BROOKESIA_STYLE_SIZE_CIRCLE_PERCENT(p)          esp_brookesia::gui::StyleSize::CIRCLE_PERCENT(p)
#define ESP_BROOKESIA_FONT_SIZE_MIN                         esp_brookesia::gui::StyleFont::FONT_SIZE_MIN
#define ESP_BROOKESIA_FONT_SIZE_MAX                         esp_brookesia::gui::StyleFont::FONT_SIZE_MAX
#define ESP_BROOKESIA_STYLE_FONT_SIZE(s)                    esp_brookesia::gui::StyleFont::SIZE(s)
#define ESP_BROOKESIA_STYLE_FONT_HEIGHT(h)                  esp_brookesia::gui::StyleFont::HEIGHT(h)
#define ESP_BROOKESIA_STYLE_FONT_HEIGHT_PERCENT(p)          esp_brookesia::gui::StyleFont::HEIGHT_PERCENT(p)
#define ESP_BROOKESIA_STYLE_FONT_CUSTOM_SIZE(s, f)          esp_brookesia::gui::StyleFont::CUSTOM_SIZE(s, f)
#define ESP_BROOKESIA_STYLE_COLOR(c)                        esp_brookesia::gui::StyleColor::COLOR(c)
#define ESP_BROOKESIA_STYLE_COLOR_WITH_OPACITY(c, o)        esp_brookesia::gui::StyleColor::COLOR_WITH_OPACITY(c, o)
#define ESP_BROOKESIA_STYLE_IMAGE(r)                        esp_brookesia::gui::StyleImage::IMAGE(r)
#define ESP_BROOKESIA_STYLE_IMAGE_RECOLOR(r, c)             esp_brookesia::gui::StyleImage::IMAGE_RECOLOR(r, c)
#define ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_WHITE(r)          esp_brookesia::gui::StyleImage::IMAGE_RECOLOR_WHITE(r)
#define ESP_BROOKESIA_STYLE_IMAGE_RECOLOR_BLACK(r)          esp_brookesia::gui::StyleImage::IMAGE_RECOLOR_BLACK(r)
#define ESP_BROOKESIA_STYLE_ALIGN(t, x, y)                  esp_brookesia::gui::StyleAlign{t, x, y}
#define ESP_BROOKESIA_STYLE_GAP_AROUND(t, b, l, r)          esp_brookesia::gui::StyleGap::AROUND(t, b, l, r)
#define ESP_BROOKESIA_STYLE_GAP_ROW(v)                      esp_brookesia::gui::StyleGap::ROW(v)
#define ESP_BROOKESIA_STYLE_GAP_COLUMN(v)                   esp_brookesia::gui::StyleGap::COLUMN(v)
#define ESP_BROOKESIA_STYLE_LAYOUT_FLEX(f, m, c, t)         esp_brookesia::gui::StyleLayoutFlex{f, m, c, t}

/**
 * @brief Backward compatibility for enum values
 */
/* StyleAlignType */
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_CENTER               esp_brookesia::gui::STYLE_ALIGN_TYPE_CENTER
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_TOP_LEFT             esp_brookesia::gui::STYLE_ALIGN_TYPE_TOP_LEFT
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_TOP_MID              esp_brookesia::gui::STYLE_ALIGN_TYPE_TOP_MID
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_TOP_RIGHT            esp_brookesia::gui::STYLE_ALIGN_TYPE_TOP_RIGHT
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_BOTTOM_LEFT          esp_brookesia::gui::STYLE_ALIGN_TYPE_BOTTOM_LEFT
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_BOTTOM_MID           esp_brookesia::gui::STYLE_ALIGN_TYPE_BOTTOM_MID
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_BOTTOM_RIGHT         esp_brookesia::gui::STYLE_ALIGN_TYPE_BOTTOM_RIGHT
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_LEFT_MID             esp_brookesia::gui::STYLE_ALIGN_TYPE_LEFT_MID
#define ESP_BROOKESIA_STYLE_ALIGN_TYPE_RIGHT_MID            esp_brookesia::gui::STYLE_ALIGN_TYPE_RIGHT_MID
/* StyleFlexFlow */
#define ESP_BROOKESIA_STYLE_FLEX_FLOW_ROW                   esp_brookesia::gui::StyleLayoutFlex::FLOW_ROW
#define ESP_BROOKESIA_STYLE_FLEX_FLOW_COLUMN                esp_brookesia::gui::StyleLayoutFlex::FLOW_COLUMN
#define ESP_BROOKESIA_STYLE_FLEX_FLOW_ROW_WRAP              esp_brookesia::gui::StyleLayoutFlex::FLOW_ROW_WRAP
#define ESP_BROOKESIA_STYLE_FLEX_FLOW_ROW_REVERSE           esp_brookesia::gui::StyleLayoutFlex::FLOW_ROW_REVERSE
#define ESP_BROOKESIA_STYLE_FLEX_FLOW_ROW_WRAP_REVERSE      esp_brookesia::gui::StyleLayoutFlex::FLOW_ROW_WRAP_REVERSE
#define ESP_BROOKESIA_STYLE_FLEX_FLOW_COLUMN_WRAP           esp_brookesia::gui::StyleLayoutFlex::FLOW_COLUMN_WRAP
#define ESP_BROOKESIA_STYLE_FLEX_FLOW_COLUMN_REVERSE        esp_brookesia::gui::StyleLayoutFlex::FLOW_COLUMN_REVERSE
#define ESP_BROOKESIA_STYLE_FLEX_FLOW_COLUMN_WRAP_REVERSE   esp_brookesia::gui::StyleLayoutFlex::FLOW_COLUMN_WRAP_REVERSE
/* StyleFlexAlign */
#define ESP_BROOKESIA_STYLE_FLEX_ALIGN_START                esp_brookesia::gui::StyleLayoutFlex::ALIGN_START
#define ESP_BROOKESIA_STYLE_FLEX_ALIGN_END                  esp_brookesia::gui::StyleLayoutFlex::ALIGN_END
#define ESP_BROOKESIA_STYLE_FLEX_ALIGN_CENTER               esp_brookesia::gui::StyleLayoutFlex::ALIGN_CENTER
#define ESP_BROOKESIA_STYLE_FLEX_ALIGN_SPACE_EVENLY         esp_brookesia::gui::StyleLayoutFlex::ALIGN_SPACE_EVENLY
#define ESP_BROOKESIA_STYLE_FLEX_ALIGN_SPACE_AROUND         esp_brookesia::gui::StyleLayoutFlex::ALIGN_SPACE_AROUND
#define ESP_BROOKESIA_STYLE_FLEX_ALIGN_SPACE_BETWEEN        esp_brookesia::gui::StyleLayoutFlex::ALIGN_SPACE_BETWEEN
/* StyleColorItem */
#define ESP_BROOKESIA_STYLE_COLOR_ITEM_BACKGROUND           esp_brookesia::gui::STYLE_COLOR_ITEM_BACKGROUND
#define ESP_BROOKESIA_STYLE_COLOR_ITEM_TEXT                 esp_brookesia::gui::STYLE_COLOR_ITEM_TEXT
/* AnimationPathType */
#define ESP_BROOKESIA_LV_ANIM_PATH_TYPE_LINEAR              esp_brookesia::gui::StyleAnimation::ANIM_PATH_TYPE_LINEAR
#define ESP_BROOKESIA_LV_ANIM_PATH_TYPE_EASE_IN             esp_brookesia::gui::StyleAnimation::ANIM_PATH_TYPE_EASE_IN
#define ESP_BROOKESIA_LV_ANIM_PATH_TYPE_EASE_OUT            esp_brookesia::gui::StyleAnimation::ANIM_PATH_TYPE_EASE_OUT
#define ESP_BROOKESIA_LV_ANIM_PATH_TYPE_EASE_IN_OUT         esp_brookesia::gui::StyleAnimation::ANIM_PATH_TYPE_EASE_IN_OUT
#define ESP_BROOKESIA_LV_ANIM_PATH_TYPE_OVERSHOOT           esp_brookesia::gui::StyleAnimation::ANIM_PATH_TYPE_OVERSHOOT
#define ESP_BROOKESIA_LV_ANIM_PATH_TYPE_BOUNCE              esp_brookesia::gui::StyleAnimation::ANIM_PATH_TYPE_BOUNCE
#define ESP_BROOKESIA_LV_ANIM_PATH_TYPE_STEP                esp_brookesia::gui::StyleAnimation::ANIM_PATH_TYPE_STEP
#define ESP_BROOKESIA_LV_ANIM_PATH_TYPE_MAX                 esp_brookesia::gui::StyleAnimation::ANIM_PATH_TYPE_MAX

/**
 * @brief Backward compatibility for type definitions
 */
using ESP_Brookesia_StyleSize_t       [[deprecated("Use `esp_brookesia::gui::StyleSize` instead")]] =
    esp_brookesia::gui::StyleSize;
using ESP_Brookesia_StyleFont_t       [[deprecated("Use `esp_brookesia::gui::StyleFont` instead")]] =
    esp_brookesia::gui::StyleFont;
using ESP_Brookesia_StyleColorItem_t  [[deprecated("Use `esp_brookesia::gui::StyleColorItem` instead")]] =
    esp_brookesia::gui::StyleColorItem;
using ESP_Brookesia_StyleColor_t      [[deprecated("Use `esp_brookesia::gui::StyleColor` instead")]] =
    esp_brookesia::gui::StyleColor;
using ESP_Brookesia_StyleImage_t      [[deprecated("Use `esp_brookesia::gui::StyleImage` instead")]] =
    esp_brookesia::gui::StyleImage;
using ESP_Brookesia_StyleAlignType_t  [[deprecated("Use `esp_brookesia::gui::StyleAlignType` instead")]] =
    esp_brookesia::gui::StyleAlignType;
using ESP_Brookesia_StyleAlign_t      [[deprecated("Use `esp_brookesia::gui::StyleAlign` instead")]] =
    esp_brookesia::gui::StyleAlign;
using ESP_Brookesia_StyleGap_t        [[deprecated("Use `esp_brookesia::gui::StyleGap` instead")]] =
    esp_brookesia::gui::StyleGap;
using ESP_Brookesia_StyleFlexFlow_t   [[deprecated("Use `esp_brookesia::gui::StyleLayoutFlex::FlowType` instead")]] =
    esp_brookesia::gui::StyleLayoutFlex::FlowType;
using ESP_Brookesia_StyleFlexAlign_t  [[deprecated("Use `esp_brookesia::gui::StyleLayoutFlex::AlignType` instead")]] =
    esp_brookesia::gui::StyleLayoutFlex::AlignType;
using ESP_Brookesia_StyleLayoutFlex_t [[deprecated("Use `esp_brookesia::gui::StyleLayoutFlex` instead")]] =
    esp_brookesia::gui::StyleLayoutFlex;
using ESP_Brookesia_AnimPathType_t    [[deprecated("Use `esp_brookesia::gui::StyleAnimation::AnimationPathType` instead")]] =
    esp_brookesia::gui::StyleAnimation::AnimationPathType;
using ESP_Brookesia_StyleAnimation_t  [[deprecated("Use `esp_brookesia::gui::StyleAnimation` instead")]] =
    esp_brookesia::gui::StyleAnimation;
