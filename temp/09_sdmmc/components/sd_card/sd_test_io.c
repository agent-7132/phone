/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/**
 * @file sd_test_io.c
 * @brief SD卡引脚测试函数实现文件
 * 
 * 本文件实现了SD卡引脚连接测试功能，用于调试SD卡通信问题：
 * 1. 测试引脚恢复时间（无外部上拉和有内部上拉两种情况）
 * 2. 测试引脚电压电平（无外部上拉和有内部上拉两种情况）
 * 3. 测试引脚串扰（不同引脚之间的信号干扰）
 * 
 * 这些测试有助于诊断SD卡连接问题，如缺少上拉电阻、引脚短路等。
 */

/* 标准库头文件 */
#include <stdio.h>              /* 标准输入输出函数 */
#include <inttypes.h>           /* 跨平台整数类型格式化宏 */
#include <unistd.h>             /* POSIX系统调用，如usleep */

/* ESP-IDF组件头文件 */
#include "esp_adc/adc_oneshot.h"  /* ADC单次采样API */
#include "driver/gpio.h"         /* GPIO驱动API */
#include "esp_cpu.h"            /* CPU相关API，如获取时钟周期 */
#include "esp_log.h"            /* ESP日志库 */

/* 项目自定义头文件 */
#include "sd_test_io.h"         /* SD卡引脚测试API声明 */

/* 日志标签，用于标识本模块的日志输出 */
const static char *TAG = "SD_TEST";

/* ADC衰减配置，设置为12dB衰减以支持更大的电压测量范围 */
#define ADC_ATTEN_DB              ADC_ATTEN_DB_12

/* GPIO输入引脚位掩码宏 */
#define GPIO_INPUT_PIN_SEL(pin)   (1ULL<<pin)

#if CONFIG_EXAMPLE_ENABLE_ADC_FEATURE

/**
 * @brief ADC校准初始化函数
 * 
 * 初始化ADC校准功能，支持曲线拟合和线性拟合两种校准方案。
 * 校准可以提高ADC测量的精度。
 * 
 * @param unit ADC单元（ADC_UNIT_1或ADC_UNIT_2）
 * @param channel ADC通道号
 * @param atten ADC衰减配置
 * @param out_handle 输出参数，存储校准句柄
 * 
 * @return bool true表示校准成功，false表示校准失败或不支持校准
 */
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    /* 校准句柄 */
    adc_cali_handle_t handle = NULL;
    
    /* 返回错误码 */
    esp_err_t ret = ESP_FAIL;
    
    /* 校准是否成功标志 */
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    /* 优先尝试曲线拟合法校准 */
    if (!calibrated) {
        /* 曲线拟合法校准配置结构体 */
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,              /* ADC单元ID */
            .chan = channel,              /* ADC通道号 */
            .atten = atten,               /* ADC衰减配置 */
            .bitwidth = ADC_BITWIDTH_DEFAULT,  /* ADC位宽（默认） */
        };
        
        /* 创建曲线拟合法校准句柄 */
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif //ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    /* 如果曲线拟合失败，尝试线性拟合法校准 */
    if (!calibrated) {
        /* 线性拟合法校准配置结构体 */
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,              /* ADC单元ID */
            .atten = atten,               /* ADC衰减配置 */
            .bitwidth = ADC_BITWIDTH_DEFAULT,  /* ADC位宽（默认） */
        };
        
        /* 创建线性拟合法校准句柄 */
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif //ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED

    /* 设置输出校准句柄 */
    *out_handle = handle;
    
    /* 检查校准结果 */
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
            /* eFuse未烧录，跳过软件校准 */
            ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
        } else {
            /* 参数无效或内存不足 */
            ESP_LOGE(TAG, "Invalid arg or no memory");
        }
    }

    return calibrated;
}

/**
 * @brief ADC校准反初始化函数
 * 
 * 释放ADC校准资源。
 * 
 * @param handle ADC校准句柄
 */
static void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    /* 删除曲线拟合法校准句柄 */
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    /* 删除线性拟合法校准句柄 */
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}

/**
 * @brief 获取引脚电压函数
 * 
 * 使用ADC测量指定引脚的电压。
 * 
 * @param i ADC通道号
 * @param adc_handle ADC单元句柄
 * @param do_calibration 是否进行校准
 * @param adc_cali_handle ADC校准句柄
 * 
 * @return float 引脚电压值（伏特）
 */
static float get_pin_voltage(int i, adc_oneshot_unit_handle_t adc_handle, bool do_calibration, adc_cali_handle_t adc_cali_handle)
{
    /* 测量电压值（毫伏） */
    int voltage = 0;
    
    /* ADC原始采样值 */
    int val;
    
    /* ADC通道配置结构体 */
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,  /* ADC位宽（默认） */
        .atten = ADC_ATTEN_DB,             /* ADC衰减配置 */
    };
    
    /* 配置ADC通道 */
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, i, &config));

    /* 读取ADC原始值 */
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, i, &val));
    
    /* 如果需要校准，将原始值转换为电压 */
    if (do_calibration) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, val, &voltage));
    }

    /* 将毫伏转换为伏特并返回 */
    return (float)voltage/1000;
}
#endif //CONFIG_EXAMPLE_ENABLE_ADC_FEATURE

/**
 * @brief 等待引脚电平变化的周期计数函数
 * 
 * 测量从当前时间开始到引脚电平变为指定值所需的CPU周期数。
 * 用于测试引脚的恢复时间。
 * 
 * @param i GPIO引脚编号
 * @param level 目标电平（0=低电平，1=高电平）
 * @param timeout 超时周期数
 * 
 * @return uint32_t 等待花费的CPU周期数
 */
static uint32_t get_cycles_until_pin_level(int i, int level, int timeout) {
    /* 记录开始时间的CPU周期数 */
    uint32_t start = esp_cpu_get_cycle_count();
    
    /* 等待引脚电平变为指定值或超时 */
    while(gpio_get_level(i) == !level && esp_cpu_get_cycle_count() - start < timeout) {
        ;
    }
    
    /* 记录结束时间的CPU周期数 */
    uint32_t end = esp_cpu_get_cycle_count();
    
    /* 返回花费的周期数 */
    return end - start;
}

/**
 * @brief 检查SD卡引脚连接函数
 * 
 * 执行SD卡引脚连接测试，包括：
 * 1. 测试引脚恢复时间（无外部上拉和有内部上拉）
 * 2. 测试引脚电压电平（无外部上拉和有内部上拉）
 * 3. 测试引脚串扰（无外部上拉和有内部上拉）
 * 
 * @param config 引脚配置结构体指针
 * @param pin_count 引脚数量
 */
void check_sd_card_pins(pin_configuration_t *config, const int pin_count)
{
    /* 打印测试开始信息 */
    ESP_LOGI(TAG, "Testing SD pin connections and pullup strength");
    
    /* GPIO配置结构体 */
    gpio_config_t io_conf = {};
    
    /* 配置所有SD卡引脚为开漏输入输出模式 */
    for (int i = 0; i < pin_count; ++i) {
        io_conf.intr_type = GPIO_INTR_DISABLE;    /* 禁用中断 */
        io_conf.mode = GPIO_MODE_INPUT_OUTPUT_OD; /* 开漏输入输出模式 */
        io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL(config->pins[i]);  /* 引脚位掩码 */
        io_conf.pull_down_en = 0;                 /* 禁用下拉电阻 */
        io_conf.pull_up_en = 0;                   /* 禁用上拉电阻 */
        gpio_config(&io_conf);
    }

    /* 打印引脚恢复时间测试标题 */
    printf("\n**** PIN recovery time ****\n\n");

    /* 测试每个引脚的恢复时间（无外部上拉） */
    for (int i = 0; i < pin_count; ++i) {
        /* 设置引脚为开漏输入输出模式 */
        gpio_set_direction(config->pins[i], GPIO_MODE_INPUT_OUTPUT_OD);
        
        /* 拉低引脚 */
        gpio_set_level(config->pins[i], 0);
        
        /* 等待100微秒稳定 */
        usleep(100);
        
        /* 释放引脚（拉高） */
        gpio_set_level(config->pins[i], 1);
        
        /* 测量引脚恢复到高电平的周期数 */
        uint32_t cycles = get_cycles_until_pin_level(config->pins[i], 1, 10000);
        
        /* 打印测试结果 */
        printf("PIN %2d %3s  %"PRIu32" cycles\n", config->pins[i], config->names[i], cycles);
    }

    /* 打印带弱上拉的引脚恢复时间测试标题 */
    printf("\n**** PIN recovery time with weak pullup ****\n\n");

    /* 测试每个引脚的恢复时间（带内部上拉） */
    for (int i = 0; i < pin_count; ++i) {
        /* 设置引脚为开漏输入输出模式 */
        gpio_set_direction(config->pins[i], GPIO_MODE_INPUT_OUTPUT_OD);
        
        /* 启用内部上拉电阻 */
        gpio_pullup_en(config->pins[i]);
        
        /* 拉低引脚 */
        gpio_set_level(config->pins[i], 0);
        
        /* 等待100微秒稳定 */
        usleep(100);
        
        /* 释放引脚（拉高） */
        gpio_set_level(config->pins[i], 1);
        
        /* 测量引脚恢复到高电平的周期数 */
        uint32_t cycles = get_cycles_until_pin_level(config->pins[i], 1, 10000);
        
        /* 打印测试结果 */
        printf("PIN %2d %3s  %"PRIu32" cycles\n", config->pins[i], config->names[i], cycles);
        
        /* 禁用内部上拉电阻 */
        gpio_pullup_dis(config->pins[i]);
    }

#if CONFIG_EXAMPLE_ENABLE_ADC_FEATURE

    /* ADC单元句柄 */
    adc_oneshot_unit_handle_t adc_handle;
    
    /* ADC单元初始化配置结构体 */
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = CONFIG_EXAMPLE_ADC_UNIT,  /* ADC单元ID */
    };
    
    /* 创建ADC单元 */
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    /* 分配校准句柄数组 */
    adc_cali_handle_t *adc_cali_handle = (adc_cali_handle_t *)malloc(sizeof(adc_cali_handle_t)*pin_count);
    
    /* 分配校准标志数组 */
    bool *do_calibration = (bool *)malloc(sizeof(bool)*pin_count);

    /* 初始化每个通道的ADC校准 */
    for (int i = 0; i < pin_count; i++) {
        do_calibration[i] = adc_calibration_init(CONFIG_EXAMPLE_ADC_UNIT, i, ADC_ATTEN_DB, &adc_cali_handle[i]);
    }

    /* 打印引脚电压电平测试标题 */
    printf("\n**** PIN voltage levels ****\n\n");

    /* 测试每个引脚的电压（无外部上拉） */
    for (int i = 0; i < pin_count; ++i) {
        /* 测量引脚电压 */
        float voltage = get_pin_voltage(config->adc_channels[i], adc_handle, do_calibration[i], adc_cali_handle[i]);
        
        /* 打印测试结果 */
        printf("PIN %2d %3s  %.1fV\n", config->pins[i], config->names[i], voltage);
    }

    /* 打印带弱上拉的引脚电压电平测试标题 */
    printf("\n**** PIN voltage levels with weak pullup ****\n\n");

    /* 测试每个引脚的电压（带内部上拉） */
    for (int i = 0; i < pin_count; ++i) {
        /* 启用内部上拉电阻 */
        gpio_pullup_en(config->pins[i]);
        
        /* 等待100微秒稳定 */
        usleep(100);
        
        /* 测量引脚电压 */
        float voltage = get_pin_voltage(config->adc_channels[i], adc_handle, do_calibration[i], adc_cali_handle[i]);
        
        /* 打印测试结果 */
        printf("PIN %2d %3s  %.1fV\n", config->pins[i], config->names[i], voltage);
        
        /* 禁用内部上拉电阻 */
        gpio_pullup_dis(config->pins[i]);
    }

    /* 打印引脚串扰测试标题 */
    printf("\n**** PIN cross-talk ****\n\n");

    /* 打印串扰测试表头 */
    printf("              ");
    for (int i = 0; i < pin_count; ++i) {
        printf("%3s   ", config->names[i]);
    }
    printf("\n");
    
    /* 测试引脚串扰（无外部上拉） */
    for (int i = 0; i < pin_count; ++i) {
        /* 设置当前引脚为开漏输入输出模式 */
        gpio_set_direction(config->pins[i], GPIO_MODE_INPUT_OUTPUT_OD);
        
        /* 拉低当前引脚 */
        gpio_set_level(config->pins[i], 0);
        
        /* 打印当前引脚信息 */
        printf("PIN %2d %3s    ", config->pins[i], config->names[i]);
        
        /* 测量其他引脚的电压变化 */
        for (int j = 0; j < pin_count; ++j) {
            if (j == i) {
                /* 跳过自己 */
                printf(" --   ");
                continue;
            }
            
            /* 等待100微秒稳定 */
            usleep(100);
            
            /* 测量受影响引脚的电压 */
            float voltage = get_pin_voltage(config->adc_channels[j], adc_handle, do_calibration[i], adc_cali_handle[i]);
            
            /* 打印测量结果 */
            printf("%1.1fV  ", voltage);
        }
        
        /* 换行 */
        printf("\n");
        
        /* 将当前引脚恢复为输入模式 */
        gpio_set_direction(config->pins[i], GPIO_MODE_INPUT);
    }

    /* 打印带弱上拉的引脚串扰测试标题 */
    printf("\n**** PIN cross-talk with weak pullup ****\n\n");

    /* 打印串扰测试表头 */
    printf("              ");
    for (int i = 0; i < pin_count; ++i) {
        printf("%3s   ", config->names[i]);
    }
    printf("\n");
    
    /* 测试引脚串扰（带内部上拉） */
    for (int i = 0; i < pin_count; ++i) {
        /* 设置当前引脚为开漏输入输出模式 */
        gpio_set_direction(config->pins[i], GPIO_MODE_INPUT_OUTPUT_OD);
        
        /* 拉低当前引脚 */
        gpio_set_level(config->pins[i], 0);
        
        /* 打印当前引脚信息 */
        printf("PIN %2d %3s    ", config->pins[i], config->names[i]);
        
        /* 测量其他引脚的电压变化 */
        for (int j = 0; j < pin_count; ++j) {
            if (j == i) {
                /* 跳过自己 */
                printf(" --   ");
                continue;
            }
            
            /* 启用受影响引脚的内部上拉 */
            gpio_pullup_en(config->pins[j]);
            
            /* 等待100微秒稳定 */
            usleep(100);
            
            /* 测量受影响引脚的电压 */
            float voltage = get_pin_voltage(config->adc_channels[j], adc_handle, do_calibration[i], adc_cali_handle[i]);
            
            /* 打印测量结果 */
            printf("%1.1fV  ", voltage);
            
            /* 禁用受影响引脚的内部上拉 */
            gpio_pullup_dis(config->pins[j]);
        }
        
        /* 换行 */
        printf("\n");
        
        /* 将当前引脚恢复为输入模式 */
        gpio_set_direction(config->pins[i], GPIO_MODE_INPUT);
    }

    /* 释放ADC校准资源 */
    for (int i = 0; i < pin_count; ++i) {
        if (do_calibration[i]) {
            example_adc_calibration_deinit(adc_cali_handle[i]);
        }
    }
#endif //CONFIG_EXAMPLE_ENABLE_ADC_FEATURE
}