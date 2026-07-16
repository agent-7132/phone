/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * @brief ESP-Brookesia手机演示应用程序入口文件
 * 
 * main.cpp是整个ESP-Brookesia手机应用的入口点，负责：
 * 1. 初始化显示和背光
 * 2. 配置GUI锁机制
 * 3. 创建Phone系统对象
 * 4. 根据屏幕分辨率选择并激活样式表
 * 5. 初始化并安装应用程序
 * 6. 创建时钟更新定时器
 * 7. 可选：创建内存信息监控线程
 */

#include "bsp/esp-bsp.h"
#include "esp_brookesia.hpp"
#include "boost/thread.hpp"
#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "Main"
#include "esp_lib_utils.h"

using namespace esp_brookesia;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems::phone;

/**
 * @brief 是否显示内存信息
 * 
 * 当设置为true时，会创建一个后台线程定期输出内存使用情况到日志和最近应用屏幕。
 */
constexpr bool EXAMPLE_SHOW_MEM_INFO = false;

/**
 * @brief ESP-IDF应用程序入口函数
 * 
 * app_main是ESP-IDF应用程序的入口点，所有初始化工作在此完成。
 */
extern "C" void app_main(void)
{
    ESP_UTILS_LOGI("Display ESP-Brookesia phone demo");

    /*
     * 如果需要使用三缓存防撕裂配置，需要修复idf 5.5。参考：
     * https://github.com/espressif/esp-iot-solution/blob/da973d162cc88736a4e05e6582393e666f221c2a/components/display/tools/esp_lvgl_adapter/README.md?plain=1#L671-L709 
     */

    /* 配置显示参数 */
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),   /*!< 使用默认LVGL适配器配置 */
        .rotation = ESP_LV_ADAPTER_ROTATE_0,                  /*!< 屏幕旋转角度：0度 */
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL, /*!< 防撕裂模式：三缓存部分刷新 */
        .touch_flags = {                                      /*!< 触摸配置 */
            .swap_xy = 0,                                     /*!< 不交换X/Y轴 */
            .mirror_x = 0,                                    /*!< X轴不镜像 */
            .mirror_y = 0}};                                  /*!< Y轴不镜像 */

    /* 启动显示驱动 */
    ESP_UTILS_CHECK_NULL_EXIT(bsp_display_start_with_config(&cfg), "Start display failed");

    /* 打开显示背光 */
    ESP_UTILS_CHECK_ERROR_EXIT(bsp_display_backlight_on(), "Turn on display backlight failed");

    /* 配置GUI锁机制 */
    LvLock::registerCallbacks(
        /* 锁回调函数：获取LVGL显示锁 */
        [](int timeout_ms) {
            esp_err_t ret = bsp_display_lock(timeout_ms);
            ESP_UTILS_CHECK_FALSE_RETURN(ret == ESP_OK, false, "Lock failed (timeout_ms: %d)", timeout_ms);
            return true;
        },
        /* 解锁回调函数：释放LVGL显示锁 */
        []() {
            bsp_display_unlock();
            return true;
        }
    );

    /* 创建Phone系统对象 */
    Phone *phone = new (std::nothrow) Phone();
    ESP_UTILS_CHECK_NULL_EXIT(phone, "Create phone failed");

    /* 根据屏幕分辨率选择对应的样式表 */
    Stylesheet *stylesheet = nullptr;
    if ((BSP_LCD_H_RES == 1024) && (BSP_LCD_V_RES == 600)) {
        /* 1024x600分辨率使用对应的深色样式表 */
        stylesheet = new (std::nothrow) Stylesheet(STYLESHEET_1024_600_DARK);
        ESP_UTILS_CHECK_NULL_EXIT(stylesheet, "Create stylesheet failed");
    } else if ((BSP_LCD_H_RES == 800) && (BSP_LCD_V_RES == 1280)) {
        /* 800x1280分辨率使用对应的深色样式表 */
        stylesheet = new (std::nothrow) Stylesheet(STYLESHEET_800_1280_DARK);
        ESP_UTILS_CHECK_NULL_EXIT(stylesheet, "Create stylesheet failed");
    }

    /* 如果成功创建了样式表，则添加并激活 */
    if (stylesheet) {
        ESP_UTILS_LOGI("Using stylesheet (%s)", stylesheet->core.name);
        ESP_UTILS_CHECK_FALSE_EXIT(phone->addStylesheet(stylesheet), "Add stylesheet failed");
        ESP_UTILS_CHECK_FALSE_EXIT(phone->activateStylesheet(stylesheet), "Activate stylesheet failed");
        delete stylesheet;  /* 样式表已复制到Phone内部，此处释放临时对象 */
    }

    {
        /* 在非GUI任务中操作LVGL前需要获取锁 */
        LvLockGuard gui_guard;

        /* 初始化Phone系统 */
        ESP_UTILS_CHECK_FALSE_EXIT(phone->begin(), "Begin failed");
        // assert(phone->getDisplay().showContainerBorder() && "Show container border failed");

        /* 从注册表初始化并安装应用程序 */
        std::vector<systems::base::Manager::RegistryAppInfo> inited_apps;
        ESP_UTILS_CHECK_FALSE_EXIT(phone->initAppFromRegistry(inited_apps), "Init app registry failed");
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installAppFromRegistry(inited_apps), "Install app registry failed");

        /* 创建定时器更新状态栏时钟 */
        lv_timer_create(
            /* 定时器回调函数：每秒更新一次时间 */
            [](lv_timer_t *t) {
                time_t now;
                struct tm timeinfo;
                Phone *phone = (Phone *)t->user_data;

                ESP_UTILS_CHECK_NULL_EXIT(phone, "Invalid phone");

                /* 获取当前时间 */
                time(&now);
                localtime_r(&now, &timeinfo);

                /* 更新状态栏时钟 */
                ESP_UTILS_CHECK_FALSE_EXIT(
                    phone->getDisplay().getStatusBar()->setClock(timeinfo.tm_hour, timeinfo.tm_min),
                    "Refresh status bar failed"
                );
            },
            1000,  /* 定时器周期：1000毫秒（1秒） */
            phone  /* 传递Phone对象作为用户数据 */
        );
    }

    /* 如果启用了内存信息显示，则创建监控线程 */
    if constexpr (EXAMPLE_SHOW_MEM_INFO) {
        esp_utils::thread_config_guard thread_config({
            .name = "mem_info",       /* 线程名称 */
            .stack_size = 4096,       /* 线程栈大小 */
        });
        boost::thread([=]() {
            char buffer[128];        /* 用于存储内存信息的缓冲区 */
            size_t internal_free = 0;   /* 内部SRAM空闲大小 */
            size_t internal_total = 0;  /* 内部SRAM总大小 */
            size_t external_free = 0;   /* 外部PSRAM空闲大小 */
            size_t external_total = 0;  /* 外部PSRAM总大小 */

            while (1) {
                /* 获取内存信息 */
                internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
                internal_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
                external_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
                external_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

                /* 格式化内存信息 */
                sprintf(buffer,
                    "\t           Biggest /     Free /    Total\n"
                    "\t  SRAM : [%8d / %8d / %8d]\n"
                    "\t PSRAM : [%8d / %8d / %8d]",
                    heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), internal_free, internal_total,
                    heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM), external_free, external_total);

                /* 输出到日志 */
                ESP_UTILS_LOGI("\n%s", buffer);

                {
                    /* 更新最近应用屏幕上的内存标签 */
                    LvLockGuard gui_guard;
                    ESP_UTILS_CHECK_FALSE_EXIT(
                        phone->getDisplay().getRecentsScreen()->setMemoryLabel(
                            internal_free / 1024, internal_total / 1024,
                            external_free / 1024, external_total / 1024
                        ), "Set memory label failed"
                    );
                }

                /* 每5秒更新一次 */
                boost::this_thread::sleep_for(boost::chrono::seconds(5));
            }
        }).detach();  /* 分离线程，让其后台运行 */
    }
}
