/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief NVS存储服务头文件
 * 
 * 提供基于ESP-IDF NVS（Non-Volatile Storage）的键值对存储服务。
 * 使用单例模式和事件驱动架构，支持线程安全的数据读写操作。
 * 数据先存储在内存中的本地参数表，然后异步写入NVS持久化存储。
 */

#pragma once

#include <bitset>
#include <queue>
#include <future>
#include <variant>
#include <string>
#include "boost/thread.hpp"
#include "boost/signals2.hpp"

namespace esp_brookesia::services {

constexpr size_t NVS_VALUE_STR_MAX_LEN = 128;  /*!< 字符串值的最大长度 */

/**
 * @brief NVS存储服务类
 * 
 * StorageNVS类提供基于NVS的键值对存储功能，支持int和string两种数据类型。
 * 使用单例模式确保全局唯一实例，通过事件队列实现异步操作，保证线程安全。
 * 
 * 核心特性：
 * - 单例模式：全局唯一实例
 * - 线程安全：使用互斥锁保护共享数据
 * - 异步操作：事件队列+后台线程处理NVS操作
 * - 信号机制：支持订阅存储事件通知
 * - 本地缓存：参数先存储在内存中，再异步写入NVS
 */
class StorageNVS {
public:
    using Key = std::string;                               /*!< 键类型（字符串） */
    using Value = std::variant<int, std::string>;          /*!< 值类型（int或string） */

    /**
     * @brief 操作类型枚举
     * 
     * 定义NVS存储服务支持的操作类型。
     */
    enum class Operation {
        UpdateNVS,    /*!< 更新NVS存储（将本地参数写入NVS） */
        UpdateParam,  /*!< 更新本地参数（从NVS读取参数到本地） */
        EraseNVS,     /*!< 擦除NVS存储 */
        Max,          /*!< 操作类型最大值，用于边界检查 */
    };

    /**
     * @brief 事件结构体
     * 
     * 定义存储操作事件的结构，用于事件通知和日志记录。
     */
    struct Event {
        /**
         * @brief 打印事件信息
         * 
         * 将事件的操作类型和键名输出到日志。
         */
        void dump() const;

        const void *sender;    /*!< 事件发送者指针（可选） */
        Operation operation;   /*!< 操作类型 */
        Key key;               /*!< 操作涉及的键名 */
    };
    using EventFuture = std::future<bool>;                  /*!< 事件操作结果的future类型 */
    using EventSignal = boost::signals2::signal<void(const Event &event)>;  /*!< 事件信号类型 */

    /**
     * @brief 删除拷贝构造函数
     * 
     * 禁止拷贝单例对象。
     */
    StorageNVS(const StorageNVS &) = delete;

    /**
     * @brief 删除移动构造函数
     * 
     * 禁止移动单例对象。
     */
    StorageNVS(StorageNVS &&) = delete;

    /**
     * @brief 析构函数
     * 
     * 默认析构，释放资源。
     */
    ~StorageNVS() = default;

    /**
     * @brief 删除拷贝赋值运算符
     * 
     * 禁止拷贝赋值单例对象。
     */
    StorageNVS &operator=(const StorageNVS &) = delete;

    /**
     * @brief 删除移动赋值运算符
     * 
     * 禁止移动赋值单例对象。
     */
    StorageNVS &operator=(StorageNVS &&) = delete;

    /**
     * @brief 初始化存储服务
     * 
     * 创建事件处理线程，初始化NVS Flash，读取已有参数到本地缓存。
     * 
     * @return true 初始化成功，false 初始化失败
     */
    bool begin();

    /**
     * @brief 发送事件到事件队列
     * 
     * 将事件加入队列，等待后台线程处理。可选返回future用于同步等待操作完成。
     * 
     * @param event 事件对象
     * @param future 事件操作结果的future指针（可选，传入后可同步等待结果）
     * @return true 发送成功，false 发送失败
     */
    bool sendEvent(const Event &event, EventFuture *future = nullptr);

    /**
     * @brief 设置本地参数
     * 
     * 将键值对存储到本地参数表，并触发事件将数据写入NVS持久化存储。
     * 
     * @param key 键名
     * @param value 值（int或string）
     * @param sender 发送者指针（可选，用于事件通知）
     * @param future 操作结果的future指针（可选）
     * @return true 设置成功，false 设置失败
     */
    bool setLocalParam(const Key &key, const Value &value, const void *sender = nullptr, EventFuture *future = nullptr);

    /**
     * @brief 获取本地参数
     * 
     * 从本地参数表中读取指定键的值。
     * 
     * @param key 键名
     * @param value 输出参数，存储读取到的值
     * @return true 读取成功，false 键不存在或读取失败
     */
    bool getLocalParam(const Key &key, Value &value);

    /**
     * @brief 擦除NVS存储
     * 
     * 擦除NVS命名空间中的所有数据。
     * 
     * @param sender 发送者指针（可选，用于事件通知）
     * @param future 操作结果的future指针（可选）
     * @return true 擦除成功，false 擦除失败
     */
    bool eraseNVS(const void *sender = nullptr, EventFuture *future = nullptr);

    /**
     * @brief 连接事件信号
     * 
     * 订阅存储事件，当存储操作完成时触发回调函数。
     * 
     * @param slot 回调函数（信号槽）
     * @return boost::signals2::connection 信号连接对象，可用于断开连接
     */
    boost::signals2::connection connectEventSignal(EventSignal::slot_type slot);

    /**
     * @brief 获取单例实例
     * 
     * 返回StorageNVS的全局唯一实例。使用Meyers单例模式，线程安全。
     * 
     * @return StorageNVS& 单例实例引用
     */
    static StorageNVS &requestInstance()
    {
        static StorageNVS instance;
        return instance;
    }

private:
    using EventPromise = std::promise<bool>;  /*!< 事件操作结果的promise类型 */

    /**
     * @brief 事件包装结构体
     * 
     * 包装事件对象和对应的promise，用于异步操作的结果传递。
     */
    struct EventWrapper {
        Event event;                                    /*!< 事件对象 */
        std::shared_ptr<EventPromise> promise;         /*!< 结果promise */
    };

    /**
     * @brief 私有构造函数
     * 
     * 单例模式，构造函数私有化，禁止外部实例化。
     */
    StorageNVS() = default;

    /**
     * @brief 处理事件
     * 
     * 根据事件类型调用对应的处理函数。
     * 
     * @param event 事件对象
     * @return true 处理成功，false 处理失败
     */
    bool processEvent(const Event &event);

    /**
     * @brief 执行更新NVS操作
     * 
     * 将指定键的本地参数写入NVS存储。
     * 
     * @param key 键名
     * @return true 更新成功，false 更新失败
     */
    bool doEventOperationUpdateNVS(const Key &key);

    /**
     * @brief 执行更新参数操作
     * 
     * 从NVS存储读取所有参数到本地参数表。
     * 
     * @return true 更新成功，false 更新失败
     */
    bool doEventOperationUpdateParam();

    /**
     * @brief 执行擦除NVS操作
     * 
     * 擦除NVS命名空间中的所有数据。
     * 
     * @return true 擦除成功，false 擦除失败
     */
    bool doEventOperationEraseNVS();

    std::map<Key, Value> _local_params;          /*!< 本地参数表（内存缓存） */
    std::mutex _params_mutex;                    /*!< 参数表互斥锁 */

    std::queue<EventWrapper> _event_queue;       /*!< 事件队列 */
    std::mutex _event_mutex;                     /*!< 事件队列互斥锁 */
    std::condition_variable _event_cv;           /*!< 事件条件变量 */
    boost::thread _event_thread;                 /*!< 事件处理线程 */
    EventSignal _event_signal;                   /*!< 事件信号（用于通知订阅者） */
};

} // namespace esp_brookesia::services
