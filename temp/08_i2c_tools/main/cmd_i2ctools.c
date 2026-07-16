/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/**
 * @file cmd_i2ctools.c
 * @brief I2C工具命令实现文件
 * 
 * 本文件实现了以下I2C工具命令：
 * - i2cconfig: 配置I2C总线参数
 * - i2cdetect: 扫描I2C总线上的设备
 * - i2cget: 读取指定设备的寄存器值
 * - i2cset: 写入指定设备的寄存器值
 * - i2cdump: 转储设备所有寄存器内容
 * 
 * 使用argtable3库解析命令行参数，提供友好的命令行接口。
 */

/* 标准库头文件 */
#include <stdio.h>              /* 标准输入输出函数 */
#include <string.h>             /* 字符串处理函数 */

/* ESP-IDF组件头文件 */
#include "argtable3/argtable3.h" /* 命令行参数解析库 */
#include "driver/i2c_master.h"  /* I2C主机驱动API */
#include "esp_console.h"        /* ESP控制台API */
#include "esp_log.h"            /* ESP日志库 */

/* 日志标签，用于标识本模块的日志输出 */
static const char *TAG = "cmd_i2ctools";

/* I2C工具超时时间（毫秒） */
#define I2C_TOOL_TIMEOUT_VALUE_MS (50)

/* I2C总线频率（Hz），默认100kHz */
static uint32_t i2c_frequency = 100 * 1000;

/* I2C主机总线句柄，用于所有I2C操作 */
i2c_master_bus_handle_t tool_bus_handle;

/**
 * @brief 获取I2C端口号
 * 
 * 验证并获取有效的I2C端口号，确保端口号在有效范围内。
 * 
 * @param port 输入的端口号
 * @param i2c_port 输出参数，存储有效的I2C端口号
 * 
 * @return esp_err_t ESP_OK表示成功，ESP_FAIL表示失败
 */
static esp_err_t i2c_get_port(int port, i2c_port_t *i2c_port)
{
    /* 检查端口号是否超出有效范围 */
    if (port >= I2C_NUM_MAX) {
        ESP_LOGE(TAG, "Wrong port number: %d", port);
        return ESP_FAIL;
    }
    
    /* 设置有效的I2C端口号 */
    *i2c_port = port;
    return ESP_OK;
}

/* i2cconfig命令参数结构体 */
static struct {
    struct arg_int *port;        /* --port选项：设置I2C端口号 */
    struct arg_int *freq;        /* --freq选项：设置I2C总线频率 */
    struct arg_int *sda;         /* --sda选项：设置SDA引脚 */
    struct arg_int *scl;         /* --scl选项：设置SCL引脚 */
    struct arg_end *end;         /* 参数列表结束标记 */
} i2cconfig_args;

/**
 * @brief i2cconfig命令实现函数
 * 
 * 配置I2C总线参数，包括端口号、频率、SDA/SCL引脚。
 * 执行流程：
 * 1. 解析命令行参数
 * 2. 删除旧的I2C总线
 * 3. 创建新的I2C总线
 * 
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * 
 * @return int 0表示成功，非0表示失败
 */
static int do_i2cconfig_cmd(int argc, char **argv)
{
    /* 解析命令行参数 */
    int nerrors = arg_parse(argc, argv, (void **)&i2cconfig_args);
    
    /* 默认I2C端口号 */
    i2c_port_t i2c_port = I2C_NUM_0;
    
    /* 默认SDA/SCL引脚 */
    int i2c_gpio_sda = 0;
    int i2c_gpio_scl = 0;
    
    /* 检查参数解析错误 */
    if (nerrors != 0) {
        arg_print_errors(stderr, i2cconfig_args.end, argv[0]);
        return 0;
    }

    /* 检查"--port"选项 */
    if (i2cconfig_args.port->count) {
        if (i2c_get_port(i2cconfig_args.port->ival[0], &i2c_port) != ESP_OK) {
            return 1;
        }
    }
    
    /* 检查"--freq"选项 */
    if (i2cconfig_args.freq->count) {
        i2c_frequency = i2cconfig_args.freq->ival[0];
    }
    
    /* 检查"--sda"选项 */
    i2c_gpio_sda = i2cconfig_args.sda->ival[0];
    
    /* 检查"--scl"选项 */
    i2c_gpio_scl = i2cconfig_args.scl->ival[0];

    /* 删除旧的I2C总线 */
    if (i2c_del_master_bus(tool_bus_handle) != ESP_OK) {
        return 1;
    }

    /* I2C总线配置结构体 */
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,         /* 时钟源：默认时钟 */
        .i2c_port = i2c_port,                      /* I2C端口号 */
        .scl_io_num = i2c_gpio_scl,                /* SCL引脚编号 */
        .sda_io_num = i2c_gpio_sda,                /* SDA引脚编号 */
        .glitch_ignore_cnt = 7,                    /* 毛刺忽略计数 */
        .flags.enable_internal_pullup = true,      /* 启用内部上拉电阻 */
    };

    /* 创建新的I2C总线 */
    if (i2c_new_master_bus(&i2c_bus_config, &tool_bus_handle) != ESP_OK) {
        return 1;
    }

    return 0;
}

/**
 * @brief 注册i2cconfig命令
 * 
 * 将i2cconfig命令注册到ESP控制台，包括命令名称、帮助信息和参数定义。
 */
static void register_i2cconfig(void)
{
    /* 定义命令行参数 */
    i2cconfig_args.port = arg_int0(NULL, "port", "<0|1>", "Set the I2C bus port number");
    i2cconfig_args.freq = arg_int0(NULL, "freq", "<Hz>", "Set the frequency(Hz) of I2C bus");
    i2cconfig_args.sda = arg_int1(NULL, "sda", "<gpio>", "Set the gpio for I2C SDA");
    i2cconfig_args.scl = arg_int1(NULL, "scl", "<gpio>", "Set the gpio for I2C SCL");
    i2cconfig_args.end = arg_end(2);
    
    /* 命令定义结构体 */
    const esp_console_cmd_t i2cconfig_cmd = {
        .command = "i2cconfig",                    /* 命令名称 */
        .help = "Config I2C bus",                  /* 帮助信息 */
        .hint = NULL,                              /* 提示信息 */
        .func = &do_i2cconfig_cmd,                /* 命令实现函数 */
        .argtable = &i2cconfig_args               /* 参数表 */
    };
    
    /* 注册命令到控制台 */
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cconfig_cmd));
}

/**
 * @brief i2cdetect命令实现函数
 * 
 * 扫描I2C总线上的所有设备地址（0x00-0x7F），检测哪些地址有设备响应。
 * 输出格式类似Linux的i2cdetect命令，显示设备地址或状态标记：
 * - 设备地址（如0x28）：表示该地址有设备响应
 * - UU：表示设备忙或超时
 * - --：表示该地址无设备响应
 * 
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * 
 * @return int 0表示成功
 */
static int do_i2cdetect_cmd(int argc, char **argv)
{
    /* 设备地址变量 */
    uint8_t address;
    
    /* 打印地址表头 */
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
    
    /* 按行扫描（每行16个地址） */
    for (int i = 0; i < 128; i += 16) {
        /* 打印行号 */
        printf("%02x: ", i);
        
        /* 扫描该行的每个地址 */
        for (int j = 0; j < 16; j++) {
            /* 刷新输出缓冲区 */
            fflush(stdout);
            
            /* 计算当前地址 */
            address = i + j;
            
            /* 探测该地址是否有设备 */
            esp_err_t ret = i2c_master_probe(tool_bus_handle, address, I2C_TOOL_TIMEOUT_VALUE_MS);
            
            /* 根据探测结果输出 */
            if (ret == ESP_OK) {
                /* 设备存在，打印地址 */
                printf("%02x ", address);
            } else if (ret == ESP_ERR_TIMEOUT) {
                /* 超时，打印UU */
                printf("UU ");
            } else {
                /* 无设备，打印-- */
                printf("-- ");
            }
        }
        /* 换行 */
        printf("\r\n");
    }

    return 0;
}

/**
 * @brief 注册i2cdetect命令
 * 
 * 将i2cdetect命令注册到ESP控制台。
 */
static void register_i2cdetect(void)
{
    /* 命令定义结构体 */
    const esp_console_cmd_t i2cdetect_cmd = {
        .command = "i2cdetect",                    /* 命令名称 */
        .help = "Scan I2C bus for devices",        /* 帮助信息 */
        .hint = NULL,                              /* 提示信息 */
        .func = &do_i2cdetect_cmd,                /* 命令实现函数 */
        .argtable = NULL                          /* 无参数 */
    };
    
    /* 注册命令到控制台 */
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cdetect_cmd));
}

/* i2cget命令参数结构体 */
static struct {
    struct arg_int *chip_address;     /* -c选项：芯片地址 */
    struct arg_int *register_address; /* -r选项：寄存器地址 */
    struct arg_int *data_length;      /* -l选项：读取长度 */
    struct arg_end *end;              /* 参数列表结束标记 */
} i2cget_args;

/**
 * @brief i2cget命令实现函数
 * 
 * 读取指定I2C设备的寄存器值。
 * 执行流程：
 * 1. 解析命令行参数
 * 2. 创建设备句柄
 * 3. 发送寄存器地址并接收数据
 * 4. 输出读取的数据
 * 
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * 
 * @return int 0表示成功，非0表示失败
 */
static int do_i2cget_cmd(int argc, char **argv)
{
    /* 解析命令行参数 */
    int nerrors = arg_parse(argc, argv, (void **)&i2cget_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, i2cget_args.end, argv[0]);
        return 0;
    }

    /* 获取芯片地址（-c选项） */
    int chip_addr = i2cget_args.chip_address->ival[0];
    
    /* 获取寄存器地址（-r选项，可选） */
    int data_addr = -1;
    if (i2cget_args.register_address->count) {
        data_addr = i2cget_args.register_address->ival[0];
    }
    
    /* 获取读取长度（-l选项，默认1字节） */
    int len = 1;
    if (i2cget_args.data_length->count) {
        len = i2cget_args.data_length->ival[0];
    }
    
    /* 分配数据缓冲区 */
    uint8_t *data = malloc(len);

    /* 设备配置结构体 */
    i2c_device_config_t i2c_dev_conf = {
        .scl_speed_hz = i2c_frequency,            /* I2C总线频率 */
        .device_address = chip_addr,               /* 设备地址 */
    };
    
    /* 设备句柄 */
    i2c_master_dev_handle_t dev_handle;
    
    /* 添加设备到总线 */
    if (i2c_master_bus_add_device(tool_bus_handle, &i2c_dev_conf, &dev_handle) != ESP_OK) {
        return 1;
    }

    /* 发送寄存器地址并接收数据 */
    esp_err_t ret = i2c_master_transmit_receive(dev_handle, (uint8_t*)&data_addr, 1, data, len, I2C_TOOL_TIMEOUT_VALUE_MS);
    
    if (ret == ESP_OK) {
        /* 读取成功，输出数据 */
        for (int i = 0; i < len; i++) {
            printf("0x%02x ", data[i]);
            /* 每16字节换行 */
            if ((i + 1) % 16 == 0) {
                printf("\r\n");
            }
        }
        /* 如果不是16的倍数，补换行 */
        if (len % 16) {
            printf("\r\n");
        }
    } else if (ret == ESP_ERR_TIMEOUT) {
        /* 超时 */
        ESP_LOGW(TAG, "Bus is busy");
    } else {
        /* 读取失败 */
        ESP_LOGW(TAG, "Read failed");
    }
    
    /* 释放数据缓冲区 */
    free(data);
    
    /* 从总线移除设备 */
    if (i2c_master_bus_rm_device(dev_handle) != ESP_OK) {
        return 1;
    }
    
    return 0;
}

/**
 * @brief 注册i2cget命令
 * 
 * 将i2cget命令注册到ESP控制台。
 */
static void register_i2cget(void)
{
    /* 定义命令行参数 */
    i2cget_args.chip_address = arg_int1("c", "chip", "<chip_addr>", "Specify the address of the chip on that bus");
    i2cget_args.register_address = arg_int0("r", "register", "<register_addr>", "Specify the address on that chip to read from");
    i2cget_args.data_length = arg_int0("l", "length", "<length>", "Specify the length to read from that data address");
    i2cget_args.end = arg_end(1);
    
    /* 命令定义结构体 */
    const esp_console_cmd_t i2cget_cmd = {
        .command = "i2cget",                       /* 命令名称 */
        .help = "Read registers visible through the I2C bus",  /* 帮助信息 */
        .hint = NULL,                              /* 提示信息 */
        .func = &do_i2cget_cmd,                   /* 命令实现函数 */
        .argtable = &i2cget_args                  /* 参数表 */
    };
    
    /* 注册命令到控制台 */
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cget_cmd));
}

/* i2cset命令参数结构体 */
static struct {
    struct arg_int *chip_address;     /* -c选项：芯片地址 */
    struct arg_int *register_address; /* -r选项：寄存器地址 */
    struct arg_int *data;             /* 数据参数 */
    struct arg_end *end;              /* 参数列表结束标记 */
} i2cset_args;

/**
 * @brief i2cset命令实现函数
 * 
 * 写入指定I2C设备的寄存器值。
 * 执行流程：
 * 1. 解析命令行参数
 * 2. 创建设备句柄
 * 3. 发送寄存器地址和数据
 * 4. 输出写入结果
 * 
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * 
 * @return int 0表示成功，非0表示失败
 */
static int do_i2cset_cmd(int argc, char **argv)
{
    /* 解析命令行参数 */
    int nerrors = arg_parse(argc, argv, (void **)&i2cset_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, i2cset_args.end, argv[0]);
        return 0;
    }

    /* 获取芯片地址（-c选项） */
    int chip_addr = i2cset_args.chip_address->ival[0];
    
    /* 获取寄存器地址（-r选项，可选） */
    int data_addr = 0;
    if (i2cset_args.register_address->count) {
        data_addr = i2cset_args.register_address->ival[0];
    }
    
    /* 获取要写入的数据长度 */
    int len = i2cset_args.data->count;

    /* 设备配置结构体 */
    i2c_device_config_t i2c_dev_conf = {
        .scl_speed_hz = i2c_frequency,            /* I2C总线频率 */
        .device_address = chip_addr,               /* 设备地址 */
    };
    
    /* 设备句柄 */
    i2c_master_dev_handle_t dev_handle;
    
    /* 添加设备到总线 */
    if (i2c_master_bus_add_device(tool_bus_handle, &i2c_dev_conf, &dev_handle) != ESP_OK) {
        return 1;
    }

    /* 分配发送缓冲区（寄存器地址+数据） */
    uint8_t *data = malloc(len + 1);
    data[0] = data_addr;                          /* 第一个字节是寄存器地址 */
    
    /* 填充数据 */
    for (int i = 0; i < len; i++) {
        data[i + 1] = i2cset_args.data->ival[i];
    }
    
    /* 发送数据 */
    esp_err_t ret = i2c_master_transmit(dev_handle, data, len + 1, I2C_TOOL_TIMEOUT_VALUE_MS);
    
    if (ret == ESP_OK) {
        /* 写入成功 */
        ESP_LOGI(TAG, "Write OK");
    } else if (ret == ESP_ERR_TIMEOUT) {
        /* 超时 */
        ESP_LOGW(TAG, "Bus is busy");
    } else {
        /* 写入失败 */
        ESP_LOGW(TAG, "Write Failed");
    }

    /* 释放数据缓冲区 */
    free(data);
    
    /* 从总线移除设备 */
    if (i2c_master_bus_rm_device(dev_handle) != ESP_OK) {
        return 1;
    }
    
    return 0;
}

/**
 * @brief 注册i2cset命令
 * 
 * 将i2cset命令注册到ESP控制台。
 */
static void register_i2cset(void)
{
    /* 定义命令行参数 */
    i2cset_args.chip_address = arg_int1("c", "chip", "<chip_addr>", "Specify the address of the chip on that bus");
    i2cset_args.register_address = arg_int0("r", "register", "<register_addr>", "Specify the address on that chip to read from");
    i2cset_args.data = arg_intn(NULL, NULL, "<data>", 0, 256, "Specify the data to write to that data address");
    i2cset_args.end = arg_end(2);
    
    /* 命令定义结构体 */
    const esp_console_cmd_t i2cset_cmd = {
        .command = "i2cset",                       /* 命令名称 */
        .help = "Set registers visible through the I2C bus",  /* 帮助信息 */
        .hint = NULL,                              /* 提示信息 */
        .func = &do_i2cset_cmd,                   /* 命令实现函数 */
        .argtable = &i2cset_args                  /* 参数表 */
    };
    
    /* 注册命令到控制台 */
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cset_cmd));
}

/* i2cdump命令参数结构体 */
static struct {
    struct arg_int *chip_address;     /* -c选项：芯片地址 */
    struct arg_int *size;             /* -s选项：读取大小 */
    struct arg_end *end;              /* 参数列表结束标记 */
} i2cdump_args;

/**
 * @brief i2cdump命令实现函数
 * 
 * 转储I2C设备的所有寄存器内容（0x00-0x7F），输出格式类似Linux的i2cdump命令。
 * 支持1字节、2字节、4字节三种读取大小。
 * 
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * 
 * @return int 0表示成功，非0表示失败
 */
static int do_i2cdump_cmd(int argc, char **argv)
{
    /* 解析命令行参数 */
    int nerrors = arg_parse(argc, argv, (void **)&i2cdump_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, i2cdump_args.end, argv[0]);
        return 0;
    }

    /* 获取芯片地址（-c选项） */
    int chip_addr = i2cdump_args.chip_address->ival[0];
    
    /* 获取读取大小（-s选项，默认1字节） */
    int size = 1;
    if (i2cdump_args.size->count) {
        size = i2cdump_args.size->ival[0];
    }
    
    /* 验证读取大小 */
    if (size != 1 && size != 2 && size != 4) {
        ESP_LOGE(TAG, "Wrong read size. Only support 1,2,4");
        return 1;
    }

    /* 设备配置结构体 */
    i2c_device_config_t i2c_dev_conf = {
        .scl_speed_hz = i2c_frequency,            /* I2C总线频率 */
        .device_address = chip_addr,               /* 设备地址 */
    };
    
    /* 设备句柄 */
    i2c_master_dev_handle_t dev_handle;
    
    /* 添加设备到总线 */
    if (i2c_master_bus_add_device(tool_bus_handle, &i2c_dev_conf, &dev_handle) != ESP_OK) {
        return 1;
    }

    /* 寄存器地址变量 */
    uint8_t data_addr;
    
    /* 数据缓冲区 */
    uint8_t data[4];
    
    /* 用于存储ASCII字符块 */
    int32_t block[16];
    
    /* 打印地址表头和ASCII表头 */
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f"
           "    0123456789abcdef\r\n");
    
    /* 按行转储（每行16个地址） */
    for (int i = 0; i < 128; i += 16) {
        /* 打印行号 */
        printf("%02x: ", i);
        
        /* 读取该行的每个地址 */
        for (int j = 0; j < 16; j += size) {
            /* 刷新输出缓冲区 */
            fflush(stdout);
            
            /* 计算当前寄存器地址 */
            data_addr = i + j;
            
            /* 发送寄存器地址并接收数据 */
            esp_err_t ret = i2c_master_transmit_receive(dev_handle, &data_addr, 1, data, size, I2C_TOOL_TIMEOUT_VALUE_MS);
            
            if (ret == ESP_OK) {
                /* 读取成功，输出数据 */
                for (int k = 0; k < size; k++) {
                    printf("%02x ", data[k]);
                    block[j + k] = data[k];
                }
            } else {
                /* 读取失败，输出XX */
                for (int k = 0; k < size; k++) {
                    printf("XX ");
                    block[j + k] = -1;
                }
            }
        }
        
        /* 输出ASCII字符 */
        printf("   ");
        for (int k = 0; k < 16; k++) {
            if (block[k] < 0) {
                printf("X");
            } else if ((block[k] & 0xff) == 0x00 || (block[k] & 0xff) == 0xff) {
                printf(".");
            } else if ((block[k] & 0xff) < 32 || (block[k] & 0xff) >= 127) {
                printf("?");
            } else {
                printf("%c", (char)(block[k] & 0xff));
            }
        }
        /* 换行 */
        printf("\r\n");
    }
    
    /* 从总线移除设备 */
    if (i2c_master_bus_rm_device(dev_handle) != ESP_OK) {
        return 1;
    }
    
    return 0;
}

/**
 * @brief 注册i2cdump命令
 * 
 * 将i2cdump命令注册到ESP控制台。
 */
static void register_i2cdump(void)
{
    /* 定义命令行参数 */
    i2cdump_args.chip_address = arg_int1("c", "chip", "<chip_addr>", "Specify the address of the chip on that bus");
    i2cdump_args.size = arg_int0("s", "size", "<size>", "Specify the size of each read");
    i2cdump_args.end = arg_end(1);
    
    /* 命令定义结构体 */
    const esp_console_cmd_t i2cdump_cmd = {
        .command = "i2cdump",                      /* 命令名称 */
        .help = "Examine registers visible through the I2C bus",  /* 帮助信息 */
        .hint = NULL,                              /* 提示信息 */
        .func = &do_i2cdump_cmd,                  /* 命令实现函数 */
        .argtable = &i2cdump_args                 /* 参数表 */
    };
    
    /* 注册命令到控制台 */
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cdump_cmd));
}

/**
 * @brief 注册所有I2C工具命令
 * 
 * 依次注册i2cconfig、i2cdetect、i2cget、i2cset、i2cdump命令。
 */
void register_i2ctools(void)
{
    register_i2cconfig();
    register_i2cdetect();
    register_i2cget();
    register_i2cset();
    register_i2cdump();
}