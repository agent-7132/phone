/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief AI表情头文件
 * 
 * 提供AI表情管理功能，包括表情动画播放、Emoji映射、系统图标显示等。
 * 通过AnimPlayer组件实现表情动画的播放和控制。
 */

#pragma once

#include <map>
#include <string>
#include "boost/thread.hpp"
#include "gui/anim_player/esp_brookesia_anim_player.hpp"

namespace esp_brookesia::ai_framework {

/**
 * @brief 表情数据结构体
 * 
 * 定义表情系统的数据配置，包括表情动画和图标动画的数据源，以及启用标志。
 */
struct ExpressionData {
    struct {
        gui::AnimPlayerData data;   /*!< 表情动画数据 */
    } emotion;
    struct {
        gui::AnimPlayerData data;   /*!< 图标动画数据 */
    } icon;
    struct {
        int enable_emotion: 1;      /*!< 启用表情动画 */
        int enable_icon: 1;         /*!< 启用图标动画 */
    } flags;                        /*!< 功能启用标志位 */
};

/**
 * @brief 表情类
 * 
 * Expression类负责管理AI表情的显示和控制，包括：
 * - Emoji到表情/图标动画的映射
 * - 表情动画的播放、暂停、恢复
 * - 临时表情的插入和自动恢复
 * - 系统图标的显示
 */
class Expression {
public:
    /**
     * @brief 动画操作配置结构体
     * 
     * 定义动画播放的配置参数，包括是否启用、是否循环、停止时是否保持、是否立即播放。
     */
    struct AnimOperationConfig {
        bool en = true;              /*!< 是否启用此动画 */
        bool repeat = true;          /*!< 是否循环播放 */
        bool keep_when_stop = false; /*!< 播放结束后是否保持最后一帧 */
        bool immediate = true;       /*!< 是否立即播放（中断当前动画） */
    };

    using EmotionType = int;                                                           /*!< 表情类型（动画索引） */
    using IconType = int;                                                              /*!< 图标类型（动画索引） */
    using EmojiMap = std::map<std::string, std::pair<EmotionType, IconType>>;          /*!< Emoji到表情/图标映射 */
    using SystemIconMap = std::map<std::string, IconType>;                              /*!< 系统图标名称到图标映射 */

    static constexpr int EMOTION_TYPE_NONE = gui::AnimPlayer::INDEX_NONE;  /*!< 无效表情类型 */
    static constexpr int ICON_TYPE_NONE = gui::AnimPlayer::INDEX_NONE;     /*!< 无效图标类型 */

    /**
     * @brief 删除拷贝构造函数
     * 
     * 禁止拷贝Expression对象。
     */
    Expression(const Expression &) = delete;

    /**
     * @brief 删除移动构造函数
     * 
     * 禁止移动Expression对象。
     */
    Expression(Expression &&) = delete;

    /**
     * @brief 删除拷贝赋值运算符
     * 
     * 禁止拷贝赋值Expression对象。
     */
    Expression &operator=(const Expression &) = delete;

    /**
     * @brief 删除移动赋值运算符
     * 
     * 禁止移动赋值Expression对象。
     */
    Expression &operator=(Expression &&) = delete;

    /**
     * @brief 默认构造函数
     * 
     * 创建Expression对象，初始化成员变量。
     */
    Expression() = default;

    /**
     * @brief 析构函数
     * 
     * 释放资源，调用del()方法清理。
     */
    ~Expression();

    /**
     * @brief 初始化表情系统
     * 
     * 创建表情和图标播放器，加载Emoji映射和系统图标映射。
     * 
     * @param data 表情数据配置
     * @param emoji_map Emoji映射表（可为nullptr，如果未启用表情）
     * @param system_icon_map 系统图标映射表（可为nullptr，如果未启用图标）
     * @return true 初始化成功，false 初始化失败
     */
    bool begin(const ExpressionData &data, const EmojiMap *emoji_map, const SystemIconMap *system_icon_map);

    /**
     * @brief 释放表情系统资源
     * 
     * 停止所有动画播放，释放播放器资源，重置状态。
     * 
     * @return true 释放成功，false 释放失败
     */
    bool del();

    /**
     * @brief 暂停表情系统
     * 
     * 暂停所有正在播放的动画，保存当前状态以便恢复。
     * 
     * @return true 暂停成功，false 暂停失败
     */
    bool pause();

    /**
     * @brief 恢复表情系统
     * 
     * 恢复暂停的动画播放，可选择性恢复表情和/或图标。
     * 
     * @param update_emotion 是否恢复表情动画
     * @param update_icon 是否恢复图标动画
     * @return true 恢复成功，false 恢复失败
     */
    bool resume(bool update_emotion, bool update_icon);

    /**
     * @brief 设置Emoji表情
     * 
     * 根据Emoji字符串查找对应的表情和图标动画并播放。
     * 
     * @param emoji Emoji字符串
     * @param emotion_config 表情动画配置
     * @param icon_config 图标动画配置
     * @return true 设置成功，false 设置失败
     */
    bool setEmoji(
        const std::string &emoji, const AnimOperationConfig &emotion_config, const AnimOperationConfig &icon_config
    );

    /**
     * @brief 设置Emoji表情（默认配置）
     * 
     * 使用默认配置设置Emoji表情。
     * 
     * @param emoji Emoji字符串
     * @return true 设置成功，false 设置失败
     */
    bool setEmoji(const std::string &emoji)
    {
        return setEmoji(emoji, AnimOperationConfig{}, AnimOperationConfig{});
    }

    /**
     * @brief 插入临时Emoji表情
     * 
     * 临时显示指定的Emoji表情，持续时间结束后自动恢复之前的表情。
     * 
     * @param emoji 临时显示的Emoji字符串
     * @param duration_ms 持续时间（毫秒，默认1000ms）
     * @return true 插入成功，false 插入失败
     */
    bool insertEmojiTemporary(const std::string &emoji, uint32_t duration_ms = 1000);

    /**
     * @brief 设置系统图标
     * 
     * 根据图标名称查找对应的图标动画并播放。
     * 
     * @param icon 图标名称
     * @param config 图标动画配置
     * @return true 设置成功，false 设置失败
     */
    bool setSystemIcon(const std::string &icon, const AnimOperationConfig &config);

    /**
     * @brief 设置系统图标（默认配置）
     * 
     * 使用默认配置设置系统图标。
     * 
     * @param icon 图标名称
     * @return true 设置成功，false 设置失败
     */
    bool setSystemIcon(const std::string &icon)
    {
        return setSystemIcon(icon, AnimOperationConfig{});
    }

private:
    /**
     * @brief 设置表情动画
     * 
     * 播放指定类型的表情动画。
     * 
     * @param type 表情类型（动画索引）
     * @param operation 播放操作（循环、单次播放等）
     * @param immediate 是否立即播放（中断当前动画）
     * @return true 设置成功，false 设置失败
     */
    bool setEmotion(EmotionType type, gui::AnimPlayer::Operation operation, bool immediate);

    /**
     * @brief 设置图标动画
     * 
     * 播放指定类型的图标动画。
     * 
     * @param type 图标类型（动画索引）
     * @param operation 播放操作（循环、单次播放等）
     * @param immediate 是否立即播放（中断当前动画）
     * @return true 设置成功，false 设置失败
     */
    bool setIcon(IconType type, gui::AnimPlayer::Operation operation, bool immediate);

    /**
     * @brief 临时Emoji定时器回调
     * 
     * 当临时Emoji持续时间结束时，恢复之前的表情。
     * 
     * @param timer 定时器句柄
     */
    static void emojiTimerCallback(TimerHandle_t timer);

    struct {
        int is_begun: 1;        /*!< 是否已初始化 */
        int is_paused: 1;       /*!< 是否已暂停 */
    } _flags = {};              /*!< 状态标志位 */
    std::mutex _mutex;          /*!< 互斥锁 */

    EmojiMap _emoji_map;                      /*!< Emoji到表情/图标映射表 */
    SystemIconMap _system_icon_map;           /*!< 系统图标名称到图标映射表 */
    std::string _last_emoji;                  /*!< 上一次设置的Emoji */
    AnimOperationConfig _last_emotion_config; /*!< 上一次的表情配置 */
    AnimOperationConfig _last_icon_config;    /*!< 上一次的图标配置 */
    TimerHandle_t timer;                      /*!< 临时Emoji定时器 */

    EmotionType _emotion_type_before_pause = EMOTION_TYPE_NONE;             /*!< 暂停前的表情类型 */
    gui::AnimPlayer::Operation _emotion_operation_before_pause = gui::AnimPlayer::Operation::PlayOnceStop; /*!< 暂停前的表情操作 */
    std::unique_ptr<gui::AnimPlayer> _emotion_player;                      /*!< 表情动画播放器 */

    IconType _icon_type_before_pause = ICON_TYPE_NONE;                      /*!< 暂停前的图标类型 */
    gui::AnimPlayer::Operation _icon_operation_before_pause = gui::AnimPlayer::Operation::PlayOnceStop; /*!< 暂停前的图标操作 */
    std::unique_ptr<gui::AnimPlayer> _icon_player;                         /*!< 图标动画播放器 */
};

} // namespace esp_brookesia::ai_framework
