/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file i2ctools_example_main.c
 * @brief I2C工具集示例程序主入口
 * 
 * 本示例实现了一个基于ESP-IDF控制台的I2C工具集，提供以下命令：
 * - i2cconfig: 配置I2C总线参数（端口、频率、SDA/SCL引脚）
 * - i2cdetect: 扫描I2C总线上的设备地址
 * - i2cget: 读取指定设备的寄存器值
 * - i2cset: 写入指定设备的寄存器值
 * - i2cdump: 转储设备所有寄存器内容
 * 
 * 使用方法：
 * 1. 通过idf.py menuconfig配置I2C SDA/SCL引脚和控制台类型
 * 2. 将程序烧录到开发板
 * 3. 打开串口终端，输入help查看所有命令
 * 4. 使用i2cdetect扫描总线上的设备
 * 
 * 关键技术点：
 * - 使用ESP-IDF控制台REPL环境提供交互式命令行
 * - 使用argtable3库解析命令行参数
 * - 支持多种控制台接口（UART、USB CDC、USB Serial JTAG）
 */

/* 标准库头文件 */
#include <stdio.h>              /* 标准输入输出函数 */
#include <string.h>             /* 字符串处理函数 */

/* ESP-IDF组件头文件 */
#include "sdkconfig.h"          /* 项目配置头文件，包含menuconfig配置选项 */
#include "esp_log.h"            /* ESP日志库 */
#include "esp_console.h"        /* ESP控制台REPL API */
#include "esp_vfs_fat.h"        /* FAT文件系统API（用于命令历史记录） */
#include "cmd_system.h"         /* 系统命令（如help） */

/* 项目自定义头文件 */
#include "cmd_i2ctools.h"       /* I2C工具命令声明 */
#include "driver/i2c_master.h"  /* I2C主机驱动API */

/* 日志标签，用于标识本模块的日志输出 */
static const char *TAG = "i2c-tools";

/* I2C SDA引脚编号，从menuconfig配置中获取 */
static gpio_num_t i2c_gpio_sda = CONFIG_EXAMPLE_I2C_MASTER_SDA;

/* I2C SCL引脚编号，从menuconfig配置中获取 */
static gpio_num_t i2c_gpio_scl = CONFIG_EXAMPLE_I2C_MASTER_SCL;

/* I2C端口号，默认使用I2C_NUM_0 */
static i2c_port_t i2c_port = I2C_NUM_0;

#if CONFIG_EXAMPLE_STORE_HISTORY

/* 文件系统挂载路径，用于存储命令历史记录 */
#define MOUNT_PATH "/data"

/* 命令历史记录文件路径 */
#define HISTORY_PATH MOUNT_PATH "/history.txt"

/**
 * @brief 初始化文件系统
 * 
 * 当启用命令历史记录功能时，需要挂载FAT文件系统来保存历史记录。
 * 该函数负责：
 * 1. 配置FAT文件系统挂载参数
 * 2. 挂载SPI Flash上的FAT文件系统
 * 3. 如果挂载失败则尝试格式化后重新挂载
 * 
 * @return void 无返回值
 */
static void initialize_filesystem(void)
{
    /* Wear leveling句柄，用于Flash磨损均衡 */
    static wl_handle_t wl_handle;
    
    /* FAT文件系统挂载配置结构体 */
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,                 /* 最大打开文件数 */
        .format_if_mount_failed = true  /* 挂载失败时自动格式化 */
    };
    
    /* 挂载SPI Flash上的FAT文件系统 */
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(MOUNT_PATH, "storage", &mount_config, &wl_handle);
    if (err != ESP_OK) {
        /* 挂载失败，记录错误日志 */
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}
#endif // CONFIG_EXAMPLE_STORE_HISTORY

/**
 * @brief ESP-IDF应用程序入口函数
 * 
 * 主函数负责初始化I2C工具集：
 * 1. 创建控制台REPL环境
 * 2. 根据配置选择控制台接口（UART/USB CDC/USB Serial JTAG）
 * 3. 初始化I2C主机总线
 * 4. 注册I2C工具命令
 * 5. 打印使用说明
 * 6. 启动控制台REPL
 * 
 * @return void 无返回值
 */
void app_main(void)
{
    /* 控制台REPL句柄 */
    esp_console_repl_t *repl = NULL;
    
    /* 控制台REPL配置结构体，使用默认配置 */
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

#if CONFIG_EXAMPLE_STORE_HISTORY
    /* 初始化文件系统（用于命令历史记录） */
    initialize_filesystem();
    
    /* 设置命令历史记录保存路径 */
    repl_config.history_save_path = HISTORY_PATH;
#endif
    
    /* 设置控制台提示符 */
    repl_config.prompt = "i2c-tools>";

    /* 根据配置选择控制台接口 */
    // 安装控制台REPL环境
#if CONFIG_ESP_CONSOLE_UART
    /* 使用UART作为控制台接口 */
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
#elif CONFIG_ESP_CONSOLE_USB_CDC
    /* 使用USB CDC作为控制台接口 */
    esp_console_dev_usb_cdc_config_t cdc_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&cdc_config, &repl_config, &repl));
#elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    /* 使用USB Serial JTAG作为控制台接口 */
    esp_console_dev_usb_serial_jtag_config_t usbjtag_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usbjtag_config, &repl_config, &repl));
#endif

    /* I2C主机总线配置结构体 */
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,         /* 时钟源：默认时钟 */
        .i2c_port = i2c_port,                      /* I2C端口号 */
        .scl_io_num = i2c_gpio_scl,                /* SCL引脚编号 */
        .sda_io_num = i2c_gpio_sda,                /* SDA引脚编号 */
        .glitch_ignore_cnt = 7,                    /* 毛刺忽略计数 */
        .flags.enable_internal_pullup = false,      /* 禁用内部上拉电阻 */
    };

    /* 创建I2C主机总线 */
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &tool_bus_handle));

    /* 注册I2C工具命令（i2cconfig、i2cdetect、i2cget、i2cset、i2cdump） */
    register_i2ctools();

    /* 打印使用说明 */
    printf("\n ==============================================================\n");
    printf(" |             Steps to Use i2c-tools                         |\n");
    printf(" |                                                            |\n");
    printf(" |  1. Try 'help', check all supported commands               |\n");
    printf(" |  2. Try 'i2cconfig' to configure your I2C bus              |\n");
    printf(" |  3. Try 'i2cdetect' to scan devices on the bus             |\n");
    printf(" |  4. Try 'i2cget' to get the content of specific register   |\n");
    printf(" |  5. Try 'i2cset' to set the value of specific register     |\n");
    printf(" |  6. Try 'i2cdump' to dump all the register (Experiment)    |\n");
    printf(" |                                                            |\n");
    printf(" ==============================================================\n\n");

    /* 启动控制台REPL，进入交互式命令行模式 */
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}