/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>

namespace esp_brookesia::systems::base {

/**
 * @class Event
 * @brief 事件管理类，提供事件注册、发送和注销功能
 * 
 * Event类是一个通用的事件管理系统，支持按对象和事件ID注册处理器，
 * 并提供灵活的事件发送机制。事件ID支持自动分配和回收复用。
 */
class Event {
public:
    /**
     * @enum ID
     * @brief 事件ID枚举
     * 
     * 预定义的事件类型，从CUSTOM开始为动态分配的事件ID。
     */
    enum class ID {
        APP,         /*!< 应用相关事件 */
        STYLESHEET,  /*!< 样式表相关事件 */
        NAVIGATION,  /*!< 导航相关事件 */
        CUSTOM,      /*!< 自定义事件起始值 */
    };

    /**
     * @struct HandlerData
     * @brief 事件处理器数据结构
     * 
     * 包含事件处理所需的所有参数，传递给事件处理器。
     */
    struct HandlerData {
        ID id;           /*!< 事件ID */
        void *object;    /*!< 触发事件的对象 */
        void *param;     /*!< 事件参数 */
        void *user_data; /*!< 用户自定义数据 */
    };

    /**
     * @brief 事件处理器函数类型
     * 
     * @param data 处理器数据
     * @return true 处理成功，false 处理失败
     */
    using Handler = bool (*)(const HandlerData &data);

    /**
     * @brief 构造函数
     */
    Event();

    /**
     * @brief 析构函数
     */
    ~Event();

    /**
     * @brief 重置事件管理器
     * 
     * 清空所有已注册的事件处理器和可用事件ID。
     */
    void reset(void);

    /**
     * @brief 注册事件处理器
     * 
     * @param object 关联的对象指针
     * @param handler 事件处理器函数指针
     * @param id 事件ID
     * @param user_data 用户自定义数据（可选）
     * @return true 成功，false 失败
     */
    bool registerEvent(void *object, Handler handler, ID id, void *user_data = nullptr);

    /**
     * @brief 发送事件
     * 
     * 触发指定对象和事件ID的所有处理器。
     * 
     * @param object 关联的对象指针
     * @param id 事件ID
     * @param param 事件参数（可选）
     * @return true 所有处理器执行成功，false 至少有一个处理器失败
     */
    bool sendEvent(void *object, ID id, void *param = nullptr) const;

    /**
     * @brief 注销指定对象的所有事件处理器
     * 
     * @param object 关联的对象指针
     */
    void unregisterEvent(void *object);

    /**
     * @brief 注销指定对象的指定事件ID的处理器
     * 
     * @param object 关联的对象指针
     * @param id 事件ID
     */
    void unregisterEvent(void *object, ID id);

    /**
     * @brief 注销指定对象的指定事件ID和处理器的组合
     * 
     * @param object 关联的对象指针
     * @param handler 事件处理器函数指针
     * @param id 事件ID
     */
    void unregisterEvent(void *object, Handler handler, ID id);

    /**
     * @brief 注销所有对象中指定事件ID的处理器
     * 
     * @param id 事件ID
     */
    void unregisterEvent(ID id);

    /**
     * @brief 注销所有对象中指定处理器的注册
     * 
     * @param handler 事件处理器函数指针
     */
    void unregisterEvent(Handler handler);

    /**
     * @brief 获取一个空闲的事件ID
     * 
     * 优先从回收的可用ID中分配，若无可用ID则递增分配新ID。
     * 
     * @return ID 可用的事件ID
     */
    ID getFreeEventID();

private:
    /**
     * @brief 事件处理器列表类型
     */
    using HandlerList = std::vector<std::pair<Handler, void *>>;

    /**
     * @brief 检查事件ID是否正在被使用
     * 
     * @param id 事件ID
     * @return true 正在使用，false 未使用
     */
    bool checkUsedEventID(ID id) const;

    /**
     * @brief 获取当前注册的事件处理器总数
     * 
     * @return size_t 处理器总数
     */
    size_t getEventHandlersCount(void) const;

    /**
     * @brief 清理空的处理器列表
     * 
     * 移除所有空的处理器列表，释放内存。
     */
    void cleanEmptyHandlers();

    ID _free_event_id; /*!< 下一个可用的事件ID */
    std::unordered_map<void *, std::unordered_map<ID, HandlerList>> _event_handlers; /*!< 对象->事件ID->处理器列表的映射 */
    std::unordered_set<ID> _available_event_ids; /*!< 可回收复用的事件ID集合 */
};

} // namespace esp_brookesia::systems::base

/**
 * @brief 向后兼容性定义
 */
using ESP_Brookesia_CoreEvent [[deprecated("Use `esp_brookesia::systems::base::Event` instead")]] =
    esp_brookesia::systems::base::Event;