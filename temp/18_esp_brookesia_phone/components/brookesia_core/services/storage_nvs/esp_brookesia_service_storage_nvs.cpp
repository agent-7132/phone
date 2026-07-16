/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief NVS存储服务实现文件
 * 
 * 实现基于ESP-IDF NVS的键值对存储服务，包括：
 * - 事件驱动的异步操作机制
 * - 线程安全的数据访问
 * - NVS Flash的初始化和管理
 * - 本地参数缓存与NVS同步
 */

#include <map>
#include <chrono>
#include "nvs_flash.h"
#include "nvs.h"
#include "private/esp_brookesia_service_storage_nvs_utils.hpp"
#include "esp_brookesia_service_storage_nvs.hpp"

#define STORAGE_NVS_PARTITION_NAME          NVS_DEFAULT_PART_NAME  /*!< NVS分区名称 */
#define STORAGE_NVS_NAMESPACE               "storage"              /*!< NVS命名空间 */

#define EVENT_THREAD_NAME                   "storage_nvs"          /*!< 事件处理线程名称 */
#define EVENT_THREAD_STACK_SIZE             (4 * 1024)             /*!< 事件处理线程栈大小（字节） */
#define EVENT_THREAD_STACK_CAPS_EXT         (false)                /*!< 是否使用外部RAM */
#define EVENT_WAIT_FINISH_TIMEOUT_MS_MAX    (60 * 60 * 1000)       /*!< 事件等待超时时间（毫秒） */

namespace esp_brookesia::services {

/**
 * @brief NVS类型到字符串的映射表
 * 
 * 用于将NVS类型枚举转换为可读的字符串，便于日志输出和调试。
 */
static const std::map<nvs_type_t, const char *> type_str_pair = {
    { NVS_TYPE_I8, "i8" },   /*!< 有符号8位整数 */
    { NVS_TYPE_U8, "u8" },   /*!< 无符号8位整数 */
    { NVS_TYPE_U16, "u16" }, /*!< 无符号16位整数 */
    { NVS_TYPE_I16, "i16" }, /*!< 有符号16位整数 */
    { NVS_TYPE_U32, "u32" }, /*!< 无符号32位整数 */
    { NVS_TYPE_I32, "i32" }, /*!< 有符号32位整数 */
    { NVS_TYPE_U64, "u64" }, /*!< 无符号64位整数 */
    { NVS_TYPE_I64, "i64" }, /*!< 有符号64位整数 */
    { NVS_TYPE_STR, "str" }, /*!< 字符串 */
    { NVS_TYPE_BLOB, "blob" }, /*!< 二进制数据 */
    { NVS_TYPE_ANY, "any" },   /*!< 任意类型 */
};

/**
 * @brief 打印事件信息
 * 
 * 将事件的操作类型和键名输出到日志，便于调试和问题追踪。
 */
void StorageNVS::Event::dump() const
{
    ESP_UTILS_LOGI(
        "{Event}:\n"
        "\t-Operation(%d)\n"
        "\t-Key(%s)\n",
        static_cast<int>(operation),
        key.empty() ? "None" : key.c_str()
    );
}

/**
 * @brief 初始化存储服务
 * 
 * 创建事件处理线程，线程内部完成以下初始化工作：
 * 1. 初始化NVS Flash（处理可能的版本更新或空间不足情况）
 * 2. 创建NVS命名空间（如果不存在）
 * 3. 进入事件循环，等待处理事件队列中的操作
 * 
 * 初始化完成后，发送UpdateParam事件从NVS读取已有参数到本地缓存。
 * 
 * @return true 初始化成功，false 初始化失败
 */
bool StorageNVS::begin()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    {
        esp_utils::thread_config_guard thread_config(esp_utils::ThreadConfig{
            .name = EVENT_THREAD_NAME,
            .stack_size = EVENT_THREAD_STACK_SIZE,
            .stack_in_ext = EVENT_THREAD_STACK_CAPS_EXT,
        });
        _event_thread = boost::thread([this]() {
            ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

            // Initialize NVS flash
            esp_err_t ret = nvs_flash_init();
            if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) || (ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
                ESP_UTILS_CHECK_FALSE_RETURN(nvs_flash_erase() == ESP_OK, false, "Erase NVS flash failed");
                ESP_UTILS_CHECK_FALSE_RETURN(nvs_flash_init() == ESP_OK, false, "Init NVS flash failed");
            } else {
                ESP_UTILS_CHECK_ERROR_RETURN(ret, false, "Initialize NVS flash failed");
            }

            // Create NVS namespace if not exists
            {
                nvs_handle_t nvs_handle;
                ESP_UTILS_CHECK_ERROR_RETURN(
                    nvs_open(STORAGE_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle), false, "Open NVS namespace failed"
                );
                esp_utils::function_guard nvs_close_guard([&]() {
                    nvs_close(nvs_handle);
                });
                ESP_UTILS_CHECK_ERROR_RETURN(nvs_commit(nvs_handle), false, "Commit NVS failed");
            }

            while (true) {
                std::unique_lock<std::mutex> lock(_event_mutex);
                _event_cv.wait(lock, [this] {
                    return !_event_queue.empty();
                });

                while (!_event_queue.empty()) {
                    auto event_wrapper = _event_queue.front();
                    _event_queue.pop();

                    lock.unlock();
                    auto ret = processEvent(event_wrapper.event);
                    lock.lock();

                    if (event_wrapper.promise != nullptr) {
                        event_wrapper.promise->set_value(ret);
                    }
                }
            }
        });
    }

    // Initialize NVS parameters
    EventFuture future;
    ESP_UTILS_CHECK_FALSE_RETURN(sendEvent({
        .operation = Operation::UpdateParam,
    }, &future), false, "Send update NVS parameters event failed");

    auto status = future.wait_for(std::chrono::milliseconds(EVENT_WAIT_FINISH_TIMEOUT_MS_MAX));
    ESP_UTILS_CHECK_FALSE_RETURN(status == std::future_status::ready, false, "Wait for update param event timeout");
    ESP_UTILS_CHECK_FALSE_RETURN(future.get(), false, "Update param event failed");

    return true;
}

/**
 * @brief 发送事件到事件队列
 * 
 * 将事件加入队列，唤醒事件处理线程处理。可选创建promise并返回future，
 * 用于同步等待操作完成。
 * 
 * @param event 事件对象
 * @param future 事件操作结果的future指针（可选）
 * @return true 发送成功，false 发送失败
 */
bool StorageNVS::sendEvent(const Event &event, EventFuture *future)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: event(%p), future(%p)", &event, future);
#if ESP_UTILS_CONF_LOG_LEVEL == ESP_UTILS_LOG_LEVEL_DEBUG
    event.dump();
#endif

    EventWrapper event_wrapper = {
        .event = event,
    };
    if (future != nullptr) {
        ESP_UTILS_CHECK_EXCEPTION_RETURN(
            event_wrapper.promise = std::make_shared<EventPromise>(), false, "Make event promise failed"
        );
        *future = event_wrapper.promise->get_future();
    }

    {
        std::lock_guard<std::mutex> lock(_event_mutex);
        _event_queue.push(event_wrapper);
        _event_cv.notify_one();
    }

    return true;
}

/**
 * @brief 设置本地参数
 * 
 * 将键值对存储到本地参数表（内存缓存），然后发送UpdateNVS事件，
 * 由后台线程将数据写入NVS持久化存储。
 * 
 * @param key 键名
 * @param value 值（int或string）
 * @param sender 发送者指针（可选）
 * @param future 操作结果的future指针（可选）
 * @return true 设置成功，false 设置失败
 */
bool StorageNVS::setLocalParam(const Key &key, const Value &value, const void *sender, EventFuture *future)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD(
        "Param: key(%s), value(%s), future(%p)", key.c_str(), std::holds_alternative<int>(value) ?
        std::to_string(std::get<int>(value)).c_str() : std::get<std::string>(value).c_str(), future
    );

    {
        std::lock_guard<std::mutex> lock(_params_mutex);
        _local_params[key] = value;
    }

    ESP_UTILS_CHECK_FALSE_RETURN(sendEvent({
        .sender = sender,
        .operation = Operation::UpdateNVS,
        .key = key,
    }, future), false, "Send update NVS event failed");

    return true;
}

/**
 * @brief 获取本地参数
 * 
 * 从本地参数表（内存缓存）中读取指定键的值。
 * 如果键不存在，返回false并输出警告日志。
 * 
 * @param key 键名
 * @param value 输出参数，存储读取到的值
 * @return true 读取成功，false 键不存在
 */
bool StorageNVS::getLocalParam(const Key &key, Value &value)
{
    std::lock_guard<std::mutex> lock(_params_mutex);

    auto it = _local_params.find(key);
    if (it == _local_params.end()) {
        ESP_UTILS_LOGW("NVS key(%s) not found", key.c_str());
        return false;
    }

    value = it->second;

    return true;
}

/**
 * @brief 擦除NVS存储
 * 
 * 发送EraseNVS事件，由后台线程擦除NVS命名空间中的所有数据。
 * 
 * @param sender 发送者指针（可选）
 * @param future 操作结果的future指针（可选）
 * @return true 擦除成功，false 擦除失败
 */
bool StorageNVS::eraseNVS(const void *sender, EventFuture *future)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: future(%p)", future);

    ESP_UTILS_CHECK_FALSE_RETURN(sendEvent({
        .sender = sender,
        .operation = Operation::EraseNVS,
    }, future), false, "Send erase NVS event failed");

    return true;
}

/**
 * @brief 连接事件信号
 * 
 * 订阅存储事件，当存储操作完成时触发回调函数。
 * 使用boost::signals2实现观察者模式。
 * 
 * @param slot 回调函数（信号槽）
 * @return boost::signals2::connection 信号连接对象，可用于断开连接
 */
boost::signals2::connection StorageNVS::connectEventSignal(EventSignal::slot_type slot)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    return _event_signal.connect(slot);
}

/**
 * @brief 处理事件
 * 
 * 根据事件类型分发到对应的处理函数：
 * - UpdateNVS: 更新NVS存储
 * - UpdateParam: 更新本地参数（从NVS读取）
 * - EraseNVS: 擦除NVS存储
 * 
 * 处理完成后，触发事件信号通知所有订阅者。
 * 
 * @param event 事件对象
 * @return true 处理成功，false 处理失败
 */
bool StorageNVS::processEvent(const Event &event)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: event(%p)", &event);
#if ESP_UTILS_CONF_LOG_LEVEL == ESP_UTILS_LOG_LEVEL_DEBUG
    event.dump();
#endif

    switch (event.operation) {
    case Operation::UpdateNVS: {
        ESP_UTILS_CHECK_FALSE_RETURN(doEventOperationUpdateNVS(event.key), false, "Update NVS failed");
        break;
    }
    case Operation::UpdateParam: {
        ESP_UTILS_CHECK_FALSE_RETURN(doEventOperationUpdateParam(), false, "Update param failed");
        break;
    }
    case Operation::EraseNVS: {
        ESP_UTILS_CHECK_FALSE_RETURN(doEventOperationEraseNVS(), false, "Erase NVS failed");
        break;
    }
    default:
        ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Invalid operation(%d)", static_cast<int>(event.operation));
    }

    _event_signal(event);

    return true;
}

/**
 * @brief 执行更新NVS操作
 * 
 * 从本地参数表中获取指定键的值，然后写入NVS存储。
 * 支持int和string两种数据类型。
 * 
 * @param key 键名
 * @return true 更新成功，false 更新失败
 */
bool StorageNVS::doEventOperationUpdateNVS(const Key &key)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: key(%s)", key.c_str());

    std::lock_guard<std::mutex> lock(_params_mutex);

    auto it = _local_params.find(key);
    ESP_UTILS_CHECK_FALSE_RETURN(
        it != _local_params.end(), false, "Invalid NVS key(%s)", key.c_str()
    );
    ESP_UTILS_LOGD("Update key(%s) NVS parameter", key.c_str());

    nvs_handle_t nvs_handle;
    ESP_UTILS_CHECK_ERROR_RETURN(
        nvs_open(STORAGE_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle), false, "Open NVS namespace failed"
    );

    esp_utils::function_guard nvs_close_guard([&]() {
        nvs_close(nvs_handle);
    });

    auto &value = it->second;
    const char *key_str = key.c_str();

    if (std::holds_alternative<int>(value)) {
        auto value_int = std::get<int>(value);
        ESP_UTILS_LOGD("Set key(%s) value(%d)", key_str, value_int);

        ESP_UTILS_CHECK_ERROR_RETURN(
            nvs_set_i32(nvs_handle, key_str, static_cast<int32_t>(value_int)), false, "Set NVS parameter failed"
        );
    } else if (std::holds_alternative<std::string>(value)) {
        auto value_str = std::get<std::string>(value);
        ESP_UTILS_LOGD("Set key(%s) value(%s)", key_str, value_str.c_str());

        ESP_UTILS_CHECK_ERROR_RETURN(
            nvs_set_str(nvs_handle, key_str, value_str.c_str()), false, "Set NVS parameter failed"
        );
    } else {
        ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Invalid NVS key(%s) value type", key_str);
    }

    ESP_UTILS_CHECK_ERROR_RETURN(nvs_commit(nvs_handle), false, "Commit NVS failed");

    return true;
}

/**
 * @brief 执行更新参数操作
 * 
 * 遍历NVS命名空间中的所有键值对，将其读取到本地参数表中。
 * 只处理i32和str两种类型的参数，其他类型跳过。
 * 
 * @return true 更新成功，false 更新失败
 */
bool StorageNVS::doEventOperationUpdateParam()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard<std::mutex> lock(_params_mutex);

    nvs_handle_t nvs_handle;
    ESP_UTILS_CHECK_ERROR_RETURN(
        nvs_open(STORAGE_NVS_NAMESPACE, NVS_READONLY, &nvs_handle), false, "Open NVS namespace failed"
    );

    esp_utils::function_guard nvs_close_guard([&]() {
        nvs_close(nvs_handle);
    });

    ESP_UTILS_LOGI("Finding keys in NVS...");

    nvs_iterator_t it = NULL;
    esp_err_t res = nvs_entry_find(STORAGE_NVS_PARTITION_NAME, STORAGE_NVS_NAMESPACE, NVS_TYPE_ANY, &it);
    while (res == ESP_OK) {
        nvs_entry_info_t info;
        res = nvs_entry_info(it, &info);
        if (res != ESP_OK) {
            ESP_UTILS_LOGE("Get key info failed");
            break;
        }

        auto type_str_it = type_str_pair.find(info.type);
        if (type_str_it == type_str_pair.end()) {
            ESP_UTILS_LOGE("\t- Invalid NVS key(%s) type(%d)", info.key, static_cast<int>(info.type));
            res = nvs_entry_next(&it);
            continue;
        }

        esp_err_t ret = ESP_OK;
        switch (info.type) {
        case NVS_TYPE_I32: {
            int32_t value_int;
            ret = nvs_get_i32(nvs_handle, info.key, &value_int);
            if (ret != ESP_OK) {
                ESP_UTILS_LOGE("\t- Get key(%s) value failed", info.key);
            } else {
                ESP_UTILS_LOGI(
                    "\t- Found key(%s): type(%s), value(%d)", info.key, type_str_it->second, static_cast<int>(value_int)
                );
                _local_params[info.key] = Value(static_cast<int>(value_int));
            }
            break;
        }
        case NVS_TYPE_STR: {
            size_t len = NVS_VALUE_STR_MAX_LEN;
            std::unique_ptr<char[]> value_str(new char[len]);
            ret = nvs_get_str(nvs_handle, info.key, value_str.get(), &len);
            if (ret != ESP_OK) {
                ESP_UTILS_LOGE("\t- Get key(%s) value failed", info.key);
            } else {
                ESP_UTILS_LOGI(
                    "\t- Found key(%s): type(%s), value(%s)", info.key, type_str_it->second, value_str.get()
                );
                _local_params[info.key] = Value(std::string(value_str.get()));
            }
            break;
        }
        default:
            ESP_UTILS_LOGI("\t- Skip key(%s): type(%s)", info.key, type_str_it->second);
            break;
        }
        res = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);

    ESP_UTILS_LOGI("Found %d keys in NVS", static_cast<int>(_local_params.size()));

    return true;
}

/**
 * @brief 执行擦除NVS操作
 * 
 * 打开NVS命名空间并擦除所有数据，然后提交更改。
 * 
 * @return true 擦除成功，false 擦除失败
 */
bool StorageNVS::doEventOperationEraseNVS()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGI("Erase NVS...");

    nvs_handle_t nvs_handle;
    ESP_UTILS_CHECK_ERROR_RETURN(
        nvs_open(STORAGE_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle), false, "Open NVS namespace failed"
    );

    esp_utils::function_guard nvs_close_guard([&]() {
        nvs_close(nvs_handle);
    });

    ESP_UTILS_CHECK_ERROR_RETURN(nvs_erase_all(nvs_handle), false, "Erase NVS failed");
    ESP_UTILS_CHECK_ERROR_RETURN(nvs_commit(nvs_handle), false, "Commit NVS failed");

    return true;
}

} // namespace esp_brookesia::services
