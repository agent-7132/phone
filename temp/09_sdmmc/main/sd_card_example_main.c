/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file sd_card_example_main.c
 * @brief SD卡和FAT文件系统示例程序
 * 
 * 本示例演示了如何使用SDMMC外设与SD卡通信，并挂载FAT文件系统：
 * 1. 初始化SDMMC主机控制器
 * 2. 配置SD卡插槽参数
 * 3. 挂载FAT文件系统
 * 4. 创建文件、写入数据、读取数据、重命名文件
 * 5. 可选格式化SD卡
 * 6. 卸载文件系统并释放资源
 * 
 * 使用方法：
 * 1. 通过idf.py menuconfig配置SD卡引脚和总线宽度
 * 2. 将SD卡插入开发板的SD卡槽
 * 3. 将程序烧录到开发板
 * 4. 打开串口监视器查看文件操作结果
 */

/* 标准库头文件 */
#include <string.h>             /* 字符串处理函数 */
#include <sys/unistd.h>         /* POSIX系统调用，如unlink */
#include <sys/stat.h>           /* 文件状态函数，如stat */

/* ESP-IDF组件头文件 */
#include "esp_vfs_fat.h"        /* FAT文件系统API */
#include "sdmmc_cmd.h"          /* SDMMC命令API */
#include "driver/sdmmc_host.h"  /* SDMMC主机驱动API */

/* 项目自定义头文件 */
#include "sd_test_io.h"         /* SD卡引脚测试API */

#if SOC_SDMMC_IO_POWER_EXTERNAL
#include "sd_pwr_ctrl_by_on_chip_ldo.h"  /* 片上LDO电源控制API */
#endif

/* 最大字符大小，用于文件读写缓冲区 */
#define EXAMPLE_MAX_CHAR_SIZE    64

/* 日志标签，用于标识本模块的日志输出 */
static const char *TAG = "example";

/* SD卡挂载点路径 */
#define MOUNT_POINT "/sdcard"

#ifdef CONFIG_EXAMPLE_DEBUG_PIN_CONNECTIONS
/* SD卡引脚名称数组 */
const char* names[] = {"CLK", "CMD", "D0", "D1", "D2", "D3"};

/* SD卡引脚编号数组 */
const int pins[] = {CONFIG_EXAMPLE_PIN_CLK,
                    CONFIG_EXAMPLE_PIN_CMD,
                    CONFIG_EXAMPLE_PIN_D0
                    #ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
                    ,CONFIG_EXAMPLE_PIN_D1,
                    CONFIG_EXAMPLE_PIN_D2,
                    CONFIG_EXAMPLE_PIN_D3
                    #endif
                    };

/* 引脚数量 */
const int pin_count = sizeof(pins)/sizeof(pins[0]);

#if CONFIG_EXAMPLE_ENABLE_ADC_FEATURE
/* ADC通道数组，用于测量引脚电压 */
const int adc_channels[] = {CONFIG_EXAMPLE_ADC_PIN_CLK,
                            CONFIG_EXAMPLE_ADC_PIN_CMD,
                            CONFIG_EXAMPLE_ADC_PIN_D0
                            #ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
                            ,CONFIG_EXAMPLE_ADC_PIN_D1,
                            CONFIG_EXAMPLE_ADC_PIN_D2,
                            CONFIG_EXAMPLE_ADC_PIN_D3
                            #endif
                            };
#endif //CONFIG_EXAMPLE_ENABLE_ADC_FEATURE

/* 引脚配置结构体 */
pin_configuration_t config = {
    .names = names,
    .pins = pins,
#if CONFIG_EXAMPLE_ENABLE_ADC_FEATURE
    .adc_channels = adc_channels,
#endif
};
#endif //CONFIG_EXAMPLE_DEBUG_PIN_CONNECTIONS

/**
 * @brief 写入文件函数
 * 
 * 创建或覆盖指定路径的文件，并写入数据。
 * 
 * @param path 文件路径
 * @param data 要写入的数据
 * 
 * @return esp_err_t ESP_OK表示成功，ESP_FAIL表示失败
 */
static esp_err_t s_example_write_file(const char *path, char *data)
{
    /* 打印打开文件信息 */
    ESP_LOGI(TAG, "Opening file %s", path);
    
    /* 以写入模式打开文件 */
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        /* 打开失败，记录错误日志 */
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    
    /* 将数据写入文件 */
    fprintf(f, data);
    
    /* 关闭文件 */
    fclose(f);
    
    /* 打印写入成功信息 */
    ESP_LOGI(TAG, "File written");

    return ESP_OK;
}

/**
 * @brief 读取文件函数
 * 
 * 打开指定路径的文件，读取第一行内容并打印。
 * 
 * @param path 文件路径
 * 
 * @return esp_err_t ESP_OK表示成功，ESP_FAIL表示失败
 */
static esp_err_t s_example_read_file(const char *path)
{
    /* 打印读取文件信息 */
    ESP_LOGI(TAG, "Reading file %s", path);
    
    /* 以只读模式打开文件 */
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        /* 打开失败，记录错误日志 */
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    
    /* 读取文件第一行内容 */
    char line[EXAMPLE_MAX_CHAR_SIZE];
    fgets(line, sizeof(line), f);
    
    /* 关闭文件 */
    fclose(f);

    /* 去除换行符 */
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    
    /* 打印读取到的内容 */
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    return ESP_OK;
}

/**
 * @brief ESP-IDF应用程序入口函数
 * 
 * 主函数负责初始化SD卡和FAT文件系统，并执行文件操作：
 * 1. 配置文件系统挂载参数
 * 2. 初始化SDMMC主机控制器
 * 3. 配置SD卡插槽
 * 4. 挂载FAT文件系统
 * 5. 打印SD卡信息
 * 6. 创建文件并写入数据
 * 7. 重命名文件
 * 8. 读取文件内容
 * 9. 可选格式化SD卡
 * 10. 卸载文件系统并释放资源
 * 
 * @return void 无返回值
 */
void app_main(void)
{
    /* 错误码变量 */
    esp_err_t ret;

    /* 文件系统挂载配置结构体 */
    /* 如果format_if_mount_failed设置为true，挂载失败时会自动分区和格式化SD卡 */
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,   /* 挂载失败时格式化 */
#else
        .format_if_mount_failed = false,  /* 挂载失败时不格式化 */
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,                    /* 最大打开文件数 */
        .allocation_unit_size = 16 * 1024  /* 分配单元大小（簇大小） */
    };
    
    /* SD卡信息结构体指针 */
    sdmmc_card_t *card;
    
    /* 挂载点路径 */
    const char mount_point[] = MOUNT_POINT;
    
    /* 打印初始化信息 */
    ESP_LOGI(TAG, "Initializing SD card");

    /* 使用上面定义的设置初始化SD卡并挂载FAT文件系统 */
    /* 注意：esp_vfs_fat_sdmmc_mount是一站式便利函数 */
    /* 在开发生产应用时，请检查其源代码并实现错误恢复 */

    /* 打印使用SDMMC外设信息 */
    ESP_LOGI(TAG, "Using SDMMC peripheral");

    /* 默认情况下，SD卡频率初始化为SDMMC_FREQ_DEFAULT（20MHz） */
    /* 要设置特定频率，使用host.max_freq_khz（SDMMC范围400kHz - 40MHz） */
    /* 例如：要设置固定频率10MHz，使用host.max_freq_khz = 10000; */
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    /* 对于SD电源可通过内部或外部（例如板载LDO）电源供应的SoC */
    /* 当使用特定IO引脚（可用于超高速SDMMC）连接SD卡并使用内部LDO电源时， */
    /* 需要先初始化电源供应 */
#if CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
    /* LDO电源控制配置结构体 */
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_IO_ID,  /* LDO通道ID */
    };
    
    /* 电源控制句柄 */
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

    /* 创建片上LDO电源控制驱动 */
    ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
        return;
    }
    
    /* 将电源控制句柄设置到主机配置 */
    host.pwr_ctrl_handle = pwr_ctrl_handle;
#endif

    /* 初始化卡槽，不使用卡检测(CD)和写保护(WP)信号 */
    /* 如果您的开发板有这些信号，请修改slot_config.gpio_cd和slot_config.gpio_wp */
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    /* 设置使用的总线宽度 */
#ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
    slot_config.width = 4;  /* 4位总线宽度 */
#else
    slot_config.width = 1;  /* 1位总线宽度 */
#endif

    /* 在可以配置SD卡GPIO的芯片上，在slot_config结构体中设置它们 */
#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
    slot_config.clk = CONFIG_EXAMPLE_PIN_CLK;  /* CLK引脚 */
    slot_config.cmd = CONFIG_EXAMPLE_PIN_CMD;  /* CMD引脚 */
    slot_config.d0 = CONFIG_EXAMPLE_PIN_D0;    /* D0引脚 */
#ifdef CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
    slot_config.d1 = CONFIG_EXAMPLE_PIN_D1;    /* D1引脚 */
    slot_config.d2 = CONFIG_EXAMPLE_PIN_D2;    /* D2引脚 */
    slot_config.d3 = CONFIG_EXAMPLE_PIN_D3;    /* D3引脚 */
#endif  // CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4
#endif  // CONFIG_SOC_SDMMC_USE_GPIO_MATRIX

    /* 启用已启用引脚的内部上拉电阻 */
    /* 但是内部上拉电阻不足，请确保总线上连接了10k外部上拉电阻 */
    /* 这仅用于调试/示例目的 */
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    /* 打印挂载文件系统信息 */
    ESP_LOGI(TAG, "Mounting filesystem");
    
    /* 挂载FAT文件系统 */
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            /* 文件系统挂载失败 */
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            /* SD卡初始化失败 */
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
#ifdef CONFIG_EXAMPLE_DEBUG_PIN_CONNECTIONS
            /* 检查SD卡引脚连接 */
            check_sd_card_pins(&config, pin_count);
#endif
        }
        return;
    }
    
    /* 打印文件系统挂载成功信息 */
    ESP_LOGI(TAG, "Filesystem mounted");

    /* SD卡已初始化，打印其属性信息 */
    sdmmc_card_print_info(stdout, card);

    /* 使用POSIX和C标准库函数处理文件： */

    /* 首先创建一个文件 */
    const char *file_hello = MOUNT_POINT"/hello.txt";
    char data[EXAMPLE_MAX_CHAR_SIZE];
    
    /* 构建写入数据（包含SD卡名称） */
    snprintf(data, EXAMPLE_MAX_CHAR_SIZE, "%s %s!\n", "Hello", card->cid.name);
    
    /* 写入文件 */
    ret = s_example_write_file(file_hello, data);
    if (ret != ESP_OK) {
        return;
    }

    /* 目标文件名 */
    const char *file_foo = MOUNT_POINT"/foo.txt";
    
    /* 重命名前检查目标文件是否存在 */
    struct stat st;
    if (stat(file_foo, &st) == 0) {
        /* 如果存在则删除 */
        unlink(file_foo);
    }

    /* 重命名原始文件 */
    ESP_LOGI(TAG, "Renaming file %s to %s", file_hello, file_foo);
    if (rename(file_hello, file_foo) != 0) {
        ESP_LOGE(TAG, "Rename failed");
        return;
    }

    /* 读取重命名后的文件 */
    ret = s_example_read_file(file_foo);
    if (ret != ESP_OK) {
        return;
    }

    /* 格式化FATFS（可选） */
#ifdef CONFIG_EXAMPLE_FORMAT_SD_CARD
    ret = esp_vfs_fat_sdcard_format(mount_point, card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format FATFS (%s)", esp_err_to_name(ret));
        return;
    }

    /* 检查格式化后文件是否存在 */
    if (stat(file_foo, &st) == 0) {
        ESP_LOGI(TAG, "file still exists");
        return;
    } else {
        ESP_LOGI(TAG, "file doesn't exist, formatting done");
    }
#endif // CONFIG_EXAMPLE_FORMAT_SD_CARD

    /* 创建新文件 */
    const char *file_nihao = MOUNT_POINT"/nihao.txt";
    
    /* 清空数据缓冲区 */
    memset(data, 0, EXAMPLE_MAX_CHAR_SIZE);
    
    /* 构建写入数据（中文问候） */
    snprintf(data, EXAMPLE_MAX_CHAR_SIZE, "%s %s!\n", "Nihao", card->cid.name);
    
    /* 写入文件 */
    ret = s_example_write_file(file_nihao, data);
    if (ret != ESP_OK) {
        return;
    }

    /* 打开文件进行读取 */
    ret = s_example_read_file(file_nihao);
    if (ret != ESP_OK) {
        return;
    }

    /* 所有操作完成，卸载分区并禁用SDMMC外设 */
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");

    /* 如果使用了电源控制驱动，取消初始化 */
#if CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
    ret = sd_pwr_ctrl_del_on_chip_ldo(pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete the on-chip LDO power control driver");
        return;
    }
#endif
}