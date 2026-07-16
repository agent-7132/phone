/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief AI Agent头文件
 * 
 * 提供AI Agent的核心类定义，包括聊天状态管理、事件处理、Coze配置等功能。
 * 使用状态机模式管理聊天会话的生命周期，支持Init、Start、Stop、Sleep、Wake等状态转换。
 */

#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <queue>
#include <future>
#include "boost/thread.hpp"
#include "boost/signals2.hpp"
#include "coze_chat_app.hpp"
#include "audio_processor.h"
#include "function_calling.hpp"

namespace esp_brookesia::ai_framework {

/**
 * @brief AI Agent类
 * 
 * Agent类是AI框架的核心组件，负责管理聊天会话的生命周期和状态转换。
 * 使用共享指针实现单例模式，通过事件队列实现异步状态转换，使用信号机制通知状态变化。
 * 
 * 状态转换流程：
 * Deinit -> Init -> Start -> Sleep/Wake -> Stop -> Deinit
 */
class Agent {
public:
    /**
     * @brief 聊天状态枚举
     * 
     * 使用位掩码方式定义聊天状态，支持状态组合判断。
     * 以_开头的状态是中间状态标记，用于状态转换的条件判断。
     */
    enum ChatState {
        ChatStateDeinit   = 0,                    /*!< 未初始化状态 */
        _ChatStateInit    = (1ULL << 0),          /*!< 初始化状态标记（内部使用） */
        ChatStateIniting  = (_ChatStateInit    | (1ULL << 1)), /*!< 初始化中 */
        ChatStateInited   = (_ChatStateInit    | (1ULL << 2)), /*!< 已初始化 */
        _ChatStateStop    = (ChatStateInited   | (1ULL << 3)), /*!< 停止状态标记（内部使用） */
        ChatStateStopping = (_ChatStateStop    | (1ULL << 4)), /*!< 停止中 */
        ChatStateStopped  = (_ChatStateStop    | (1ULL << 5)), /*!< 已停止 */
        _ChatStateStart   = (ChatStateInited   | (1ULL << 6)), /*!< 启动状态标记（内部使用） */
        ChatStateStarting = (_ChatStateStart   | (1ULL << 7)), /*!< 启动中 */
        ChatStateStarted  = (_ChatStateStart   | (1ULL << 8)), /*!< 已启动 */
        _ChatStateSleep   = (ChatStateStarted  | (1ULL << 9)), /*!< 睡眠状态标记（内部使用） */
        ChatStateSleeping = (_ChatStateSleep   | (1ULL << 10)), /*!< 睡眠中 */
        ChatStateSlept    = (_ChatStateSleep   | (1ULL << 11)), /*!< 已睡眠 */
        _ChatStateWake    = (ChatStateStarted  | (1ULL << 12)), /*!< 唤醒状态标记（内部使用） */
        ChatStateWaking   = (_ChatStateWake    | (1ULL << 13)), /*!< 唤醒中 */
        ChatStateWaked    = (_ChatStateWake    | (1ULL << 14)), /*!< 已唤醒 */
    };

    /**
     * @brief 聊天事件枚举
     * 
     * 定义触发状态转换的事件类型。
     */
    enum class ChatEvent {
        Deinit,   /*!< 触发反初始化 */
        Init,     /*!< 触始初始化 */
        Stop,     /*!< 触发停止 */
        Start,    /*!< 触发启动 */
        Sleep,    /*!< 触发睡眠 */
        WakeUp,   /*!< 触发唤醒 */
    };

    /**
     * @brief 聊天事件特殊信号类型
     * 
     * 定义处理聊天事件时的特殊信号，用于通知错误或异常情况。
     */
    enum class ChatEventSpecialSignalType {
        InitInvalidConfig, /*!< 初始化时配置无效 */
        StartMaxRetry,     /*!< 启动时达到最大重试次数 */
    };

    using ChatEventProcessSpecialSignal = boost::signals2::signal<void(const ChatEventSpecialSignalType &type)>; /*!< 特殊信号类型 */
    using ChatEventProcessStartSignal = boost::signals2::signal<void(const ChatEvent &current_event, const ChatEvent &last_event)>; /*!< 事件处理开始信号 */
    using ChatEventProcessEndSignal = boost::signals2::signal<void(const ChatEvent &current_event, const ChatEvent &last_event)>; /*!< 事件处理结束信号 */

    /**
     * @brief 删除拷贝构造函数
     * 
     * 禁止拷贝Agent对象。
     */
    Agent(const Agent &) = delete;

    /**
     * @brief 删除移动构造函数
     * 
     * 禁止移动Agent对象。
     */
    Agent(Agent &&) = delete;

    /**
     * @brief 删除拷贝赋值运算符
     * 
     * 禁止拷贝赋值Agent对象。
     */
    Agent &operator=(const Agent &) = delete;

    /**
     * @brief 删除移动赋值运算符
     * 
     * 禁止移动赋值Agent对象。
     */
    Agent &operator=(Agent &&) = delete;

    /**
     * @brief 析构函数
     * 
     * 释放资源，调用del()方法清理。
     */
    ~Agent();

    /**
     * @brief 配置Coze Agent信息
     * 
     * 设置Agent的配置信息，包括会话名称、设备ID、用户ID、密钥等。
     * 如果未提供会话名称、设备ID或用户ID，将自动使用MAC地址生成。
     * 
     * @param agent_info Agent配置信息
     * @param robot_infos 机器人配置信息列表
     * @return true 配置成功，false 配置失败
     */
    bool configCozeAgentConfig(CozeChatAgentInfo &agent_info, std::vector<CozeChatRobotInfo> &robot_infos);

    /**
     * @brief 初始化Agent
     * 
     * 创建事件处理线程，注册信号连接，准备接收聊天事件。
     * 
     * @return true 初始化成功，false 初始化失败
     */
    bool begin();

    /**
     * @brief 释放Agent资源
     * 
     * 断开所有信号连接，停止聊天会话，清空事件队列，重置状态。
     * 
     * @return true 释放成功，false 释放失败
     */
    bool del();

    /**
     * @brief 暂停Agent
     * 
     * 暂停聊天会话，保留当前状态。
     * 
     * @return true 暂停成功，false 暂停失败
     */
    bool pause();

    /**
     * @brief 恢复Agent
     * 
     * 恢复暂停的聊天会话。
     * 
     * @return true 恢复成功，false 恢复失败
     */
    bool resume();

    /**
     * @brief 设置当前机器人索引
     * 
     * 在多个机器人之间切换。
     * 
     * @param index 机器人索引（从0开始）
     * @return true 设置成功，false 索引无效
     */
    bool setCurrentRobotIndex(int index);

    /**
     * @brief 获取当前机器人索引
     * 
     * @param index 输出参数，存储当前机器人索引
     * @return true 获取成功
     */
    bool getCurrentRobotIndex(int &index) const;

    /**
     * @brief 获取指定索引的机器人信息
     * 
     * @param index 机器人索引
     * @param info 输出参数，存储机器人信息
     * @return true 获取成功，false 索引无效
     */
    bool getRobotInfo(int index, CozeChatRobotInfo &info) const;

    /**
     * @brief 获取所有机器人信息
     * 
     * @param infos 输出参数，存储所有机器人信息
     * @return true 获取成功
     */
    bool getRobotInfo(std::vector<CozeChatRobotInfo> &infos) const;

    /**
     * @brief 发送聊天事件
     * 
     * 将聊天事件加入队列，等待事件处理线程处理。
     * 
     * @param event 聊天事件类型
     * @param clear_queue 是否清空队列中已有事件（默认true）
     * @param wait_finish_timeout_ms 等待事件处理完成的超时时间（毫秒，0表示不等待）
     * @return true 发送成功，false 发送失败或超时
     */
    bool sendChatEvent(const ChatEvent &event, bool clear_queue = true, int wait_finish_timeout_ms = 0);

    /**
     * @brief 检查是否包含指定状态
     * 
     * 使用位掩码判断当前状态是否包含指定状态。
     * 
     * @param state 要检查的状态
     * @return true 包含指定状态，false 不包含
     */
    bool hasChatState(ChatState state) const
    {
        return (_chat_state & state) == state;
    }

    /**
     * @brief 检查当前状态是否等于指定状态
     * 
     * @param state 要检查的状态
     * @return true 当前状态等于指定状态，false 不等
     */
    bool isChatState(ChatState state) const
    {
        return (_chat_state == state);
    }

    /**
     * @brief 检查Agent是否处于暂停状态
     * 
     * @return true 已暂停，false 未暂停
     */
    bool isPaused() const
    {
        return _flags.is_paused;
    }

    /**
     * @brief 获取Agent单例实例
     * 
     * 使用双重检查锁定模式创建和获取单例实例。
     * 
     * @return std::shared_ptr<Agent> Agent实例的共享指针
     */
    static std::shared_ptr<Agent> requestInstance();

    /**
     * @brief 释放Agent单例实例
     * 
     * 当引用计数为1时，释放实例。
     */
    static void releaseInstance();

    /**
     * @brief 将聊天事件转换为字符串
     * 
     * @param event 聊天事件
     * @return std::string 事件名称字符串
     */
    static std::string chatEventToString(const ChatEvent &event);

    /**
     * @brief 将聊天状态转换为字符串
     * 
     * @param state 聊天状态
     * @return std::string 状态名称字符串
     */
    static std::string chatStateToString(const ChatState &state);

    ChatEventProcessSpecialSignal chat_event_process_special_signal; /*!< 特殊信号（用于通知错误） */
    ChatEventProcessStartSignal chat_event_process_start_signal;     /*!< 事件处理开始信号 */
    ChatEventProcessEndSignal chat_event_process_end_signal;         /*!< 事件处理结束信号 */

private:
    using EventPromise = std::promise<bool>; /*!< 事件处理结果的promise类型 */

    /**
     * @brief 聊天事件包装结构体
     * 
     * 包装事件对象和对应的promise，用于异步操作的结果传递。
     */
    struct ChatEventWrapper {
        ChatEvent event;                           /*!< 聊天事件 */
        std::shared_ptr<EventPromise> promise;     /*!< 结果promise */
    };

    /**
     * @brief 私有构造函数
     * 
     * 单例模式，构造函数私有化，禁止外部实例化。
     */
    Agent() = default;

    /**
     * @brief 处理聊天事件
     * 
     * 根据事件类型执行对应的操作，包括状态转换和业务逻辑。
     * 
     * @param event 聊天事件
     * @return true 处理成功，false 处理失败
     */
    bool processChatEvent(const ChatEvent &event);

    /**
     * @brief 检查时间是否同步
     * 
     * 判断系统时间是否已同步（年份大于2020）。
     * 
     * @return true 时间已同步，false 时间未同步
     */
    static bool isTimeSync();

    /**
     * @brief 获取MAC地址字符串
     * 
     * 获取设备的MAC地址并格式化为字符串（ESP_+十六进制MAC）。
     * 
     * @param mac_str 输出参数，存储MAC地址字符串
     * @return true 获取成功，false 获取失败
     */
    static bool getMacStr(std::string &mac_str);

    struct {
        int is_begun: 1;        /*!< 是否已初始化 */
        int is_paused: 1;       /*!< 是否已暂停 */
        int is_coze_error: 1;   /*!< 是否发生Coze错误（如余额不足） */
    } _flags = {};              /*!< 状态标志位 */
    std::mutex _mutex;          /*!< 互斥锁 */

    CozeChatAgentInfo _agent_info = {};            /*!< Agent配置信息 */
    std::vector<CozeChatRobotInfo> _robot_infos;   /*!< 机器人配置信息列表 */
    int _robot_index = 0;                          /*!< 当前机器人索引 */

    ChatState _chat_state = ChatState::ChatStateDeinit;   /*!< 当前聊天状态 */
    ChatEvent _last_chat_event = ChatEvent::Deinit;       /*!< 上一次处理的聊天事件 */
    boost::thread _chat_event_thread;                    /*!< 事件处理线程 */
    std::queue<ChatEventWrapper> _chat_event_queue;       /*!< 事件队列 */
    std::recursive_mutex _chat_event_mutex;               /*!< 事件队列互斥锁 */
    boost::condition_variable_any _chat_event_cv;         /*!< 事件条件变量 */

    std::vector<boost::signals2::connection> _connections; /*!< 信号连接列表 */

    inline static std::mutex _instance_mutex;      /*!< 单例实例互斥锁 */
    inline static std::shared_ptr<Agent> _instance; /*!< 单例实例 */
};

} // namespace esp_brookesia::ai_agent
