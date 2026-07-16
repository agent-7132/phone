/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/**
 * @file ethernet_init.c
 * @brief 以太网初始化组件实现文件
 * 
 * 本组件提供以太网驱动的初始化和反初始化功能，支持两种以太网接口：
 * 1. 内部以太网控制器（ESP32 EMAC）
 * 2. SPI以太网模块（KSZ8851SNL、DM9051、W5500）
 * 
 * 主要功能：
 * - 根据配置自动选择以太网接口类型
 * - 配置MAC和PHY参数
 * - 管理多端口以太网配置
 * - 提供统一的初始化和反初始化接口
 */

#include "ethernet_init.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_mac.h"
#include "esp_idf_version.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#if CONFIG_EXAMPLE_USE_SPI_ETHERNET
#include "driver/spi_master.h"
#endif

#if CONFIG_EXAMPLE_ETH_PHY_IP101 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#include "esp_eth_phy_ip101.h"
#endif

#if CONFIG_EXAMPLE_SPI_ETHERNETS_NUM
#define SPI_ETHERNETS_NUM           CONFIG_EXAMPLE_SPI_ETHERNETS_NUM
#else
#define SPI_ETHERNETS_NUM           0
#endif

#if CONFIG_EXAMPLE_USE_INTERNAL_ETHERNET
#define INTERNAL_ETHERNETS_NUM      1
#else
#define INTERNAL_ETHERNETS_NUM      0
#endif

/**
 * @def INIT_SPI_ETH_MODULE_CONFIG
 * @brief 初始化SPI以太网模块配置宏
 * 
 * 该宏用于从Kconfig配置中读取指定SPI以太网模块的参数，包括：
 * - SPI片选GPIO
 * - 中断GPIO
 * - 轮询周期（毫秒）
 * - PHY复位GPIO
 * - PHY地址
 * 
 * @param eth_module_config SPI以太网模块配置数组
 * @param num 模块编号（0或1）
 */
#define INIT_SPI_ETH_MODULE_CONFIG(eth_module_config, num)                                      \
    do {                                                                                        \
        eth_module_config[num].spi_cs_gpio = CONFIG_EXAMPLE_ETH_SPI_CS ##num## _GPIO;           \
        eth_module_config[num].int_gpio = CONFIG_EXAMPLE_ETH_SPI_INT ##num## _GPIO;             \
        eth_module_config[num].polling_ms = CONFIG_EXAMPLE_ETH_SPI_POLLING ##num## _MS;         \
        eth_module_config[num].phy_reset_gpio = CONFIG_EXAMPLE_ETH_SPI_PHY_RST ##num## _GPIO;   \
        eth_module_config[num].phy_addr = CONFIG_EXAMPLE_ETH_SPI_PHY_ADDR ##num;                \
    } while(0)

/**
 * @brief SPI以太网模块配置结构体
 * 
 * 包含单个SPI以太网模块的所有配置参数，用于初始化SPI以太网驱动。
 */
typedef struct {
    uint8_t spi_cs_gpio;          /**< SPI片选GPIO编号 */
    int8_t int_gpio;              /**< 中断GPIO编号，-1表示不使用中断 */
    uint32_t polling_ms;          /**< 轮询周期（毫秒），用于无中断模式 */
    int8_t phy_reset_gpio;        /**< PHY复位GPIO编号，-1表示不使用复位 */
    uint8_t phy_addr;             /**< PHY地址 */
    uint8_t *mac_addr;            /**< MAC地址指针，NULL表示使用默认地址 */
}spi_eth_module_config_t;

static const char *TAG = "example_eth_init";
#if CONFIG_EXAMPLE_USE_SPI_ETHERNET
static bool gpio_isr_svc_init_by_eth = false; /**< 标记GPIO ISR服务是否由以太网组件初始化 */
#endif


#if CONFIG_EXAMPLE_USE_INTERNAL_ETHERNET
/**
 * @brief 内部ESP32以太网控制器初始化函数
 * 
 * 初始化ESP32内部以太网MAC控制器和PHY芯片，支持多种PHY芯片型号：
 * - IP101
 * - RTL8201
 * - LAN87XX
 * - DP83848
 * - KSZ80XX
 * 
 * @param[out] mac_out 可选输出参数，返回创建的MAC对象指针
 * @param[out] phy_out 可选输出参数，返回创建的PHY对象指针
 * 
 * @return 
 *          - esp_eth_handle_t 以太网驱动句柄，初始化成功
 *          - NULL 初始化失败
 */
static esp_eth_handle_t eth_init_internal(esp_eth_mac_t **mac_out, esp_eth_phy_t **phy_out)
{
    esp_eth_handle_t ret = NULL;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    phy_config.phy_addr = CONFIG_EXAMPLE_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;

    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    esp32_emac_config.smi_gpio.mdc_num = CONFIG_EXAMPLE_ETH_MDC_GPIO;
    esp32_emac_config.smi_gpio.mdio_num = CONFIG_EXAMPLE_ETH_MDIO_GPIO;

#if CONFIG_EXAMPLE_USE_SPI_ETHERNET
    esp32_emac_config.dma_burst_len = ETH_DMA_BURST_LEN_4;
#endif

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);

#if CONFIG_EXAMPLE_ETH_PHY_IP101
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
#elif CONFIG_EXAMPLE_ETH_PHY_RTL8201
    esp_eth_phy_t *phy = esp_eth_phy_new_rtl8201(&phy_config);
#elif CONFIG_EXAMPLE_ETH_PHY_LAN87XX
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);
#elif CONFIG_EXAMPLE_ETH_PHY_DP83848
    esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
#elif CONFIG_EXAMPLE_ETH_PHY_KSZ80XX
    esp_eth_phy_t *phy = esp_eth_phy_new_ksz80xx(&phy_config);
#endif

    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_GOTO_ON_FALSE(esp_eth_driver_install(&config, &eth_handle) == ESP_OK, NULL,
                        err, TAG, "Ethernet driver install failed");

    if (mac_out != NULL) {
        *mac_out = mac;
    }
    if (phy_out != NULL) {
        *phy_out = phy;
    }
    return eth_handle;

err:
    if (eth_handle != NULL) {
        esp_eth_driver_uninstall(eth_handle);
    }
    if (mac != NULL) {
        mac->del(mac);
    }
    if (phy != NULL) {
        phy->del(phy);
    }
    return ret;
}
#endif


#if CONFIG_EXAMPLE_USE_SPI_ETHERNET
/**
 * @brief SPI总线初始化函数
 * 
 * 初始化SPI总线以用于SPI以太网模块通信，执行以下操作：
 * 1. 安装GPIO ISR服务（如果需要中断）
 * 2. 配置SPI总线参数（MISO、MOSI、SCLK引脚）
 * 3. 初始化SPI主机控制器
 * 
 * @return 
 *          - ESP_OK 初始化成功
 *          - ESP_ERR_INVALID_STATE GPIO ISR服务已被安装（但仍返回成功）
 *          - 其他错误码 初始化失败
 */
static esp_err_t spi_bus_init(void)
{
    esp_err_t ret = ESP_OK;

#if (CONFIG_EXAMPLE_ETH_SPI_INT0_GPIO >= 0) || (CONFIG_EXAMPLE_ETH_SPI_INT1_GPIO > 0)
    ret = gpio_install_isr_service(0);
    if (ret == ESP_OK) {
        gpio_isr_svc_init_by_eth = true;
    } else if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "GPIO ISR handler has been already installed");
        ret = ESP_OK;
    } else {
        ESP_LOGE(TAG, "GPIO ISR handler install failed");
        goto err;
    }
#endif

    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_EXAMPLE_ETH_SPI_MISO_GPIO,
        .mosi_io_num = CONFIG_EXAMPLE_ETH_SPI_MOSI_GPIO,
        .sclk_io_num = CONFIG_EXAMPLE_ETH_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_GOTO_ON_ERROR(spi_bus_initialize(CONFIG_EXAMPLE_ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO),
                        err, TAG, "SPI host #%d init failed", CONFIG_EXAMPLE_ETH_SPI_HOST);

err:
    return ret;
}

/**
 * @brief SPI以太网模块初始化函数
 * 
 * 初始化指定的SPI以太网模块，支持多种芯片型号：
 * - KSZ8851SNL
 * - DM9051
 * - W5500
 * 
 * 主要操作：
 * 1. 配置MAC和PHY参数
 * 2. 配置SPI设备接口
 * 3. 创建MAC和PHY实例
 * 4. 安装以太网驱动
 * 5. 设置MAC地址（如果配置了）
 * 
 * @param[in] spi_eth_module_config SPI以太网模块配置参数
 * @param[out] mac_out 可选输出参数，返回创建的MAC对象指针
 * @param[out] phy_out 可选输出参数，返回创建的PHY对象指针
 * 
 * @return 
 *          - esp_eth_handle_t 以太网驱动句柄，初始化成功
 *          - NULL 初始化失败
 */
static esp_eth_handle_t eth_init_spi(spi_eth_module_config_t *spi_eth_module_config, esp_eth_mac_t **mac_out, esp_eth_phy_t **phy_out)
{
    esp_eth_handle_t ret = NULL;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    phy_config.phy_addr = spi_eth_module_config->phy_addr;
    phy_config.reset_gpio_num = spi_eth_module_config->phy_reset_gpio;

    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = CONFIG_EXAMPLE_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size = 20,
        .spics_io_num = spi_eth_module_config->spi_cs_gpio
    };

#if CONFIG_EXAMPLE_USE_KSZ8851SNL
    eth_ksz8851snl_config_t ksz8851snl_config = ETH_KSZ8851SNL_DEFAULT_CONFIG(CONFIG_EXAMPLE_ETH_SPI_HOST, &spi_devcfg);
    ksz8851snl_config.int_gpio_num = spi_eth_module_config->int_gpio;
    ksz8851snl_config.poll_period_ms = spi_eth_module_config->polling_ms;
    esp_eth_mac_t *mac = esp_eth_mac_new_ksz8851snl(&ksz8851snl_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_ksz8851snl(&phy_config);
#elif CONFIG_EXAMPLE_USE_DM9051
    eth_dm9051_config_t dm9051_config = ETH_DM9051_DEFAULT_CONFIG(CONFIG_EXAMPLE_ETH_SPI_HOST, &spi_devcfg);
    dm9051_config.int_gpio_num = spi_eth_module_config->int_gpio;
    dm9051_config.poll_period_ms = spi_eth_module_config->polling_ms;
    esp_eth_mac_t *mac = esp_eth_mac_new_dm9051(&dm9051_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_dm9051(&phy_config);
#elif CONFIG_EXAMPLE_USE_W5500
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(CONFIG_EXAMPLE_ETH_SPI_HOST, &spi_devcfg);
    w5500_config.int_gpio_num = spi_eth_module_config->int_gpio;
    w5500_config.poll_period_ms = spi_eth_module_config->polling_ms;
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
#endif

    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t eth_config_spi = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_GOTO_ON_FALSE(esp_eth_driver_install(&eth_config_spi, &eth_handle) == ESP_OK, NULL, err, TAG, "SPI Ethernet driver install failed");

    if (spi_eth_module_config->mac_addr != NULL) {
        ESP_GOTO_ON_FALSE(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, spi_eth_module_config->mac_addr) == ESP_OK,
                                        NULL, err, TAG, "SPI Ethernet MAC address config failed");
    }

    if (mac_out != NULL) {
        *mac_out = mac;
    }
    if (phy_out != NULL) {
        *phy_out = phy;
    }
    return eth_handle;

err:
    if (eth_handle != NULL) {
        esp_eth_driver_uninstall(eth_handle);
    }
    if (mac != NULL) {
        mac->del(mac);
    }
    if (phy != NULL) {
        phy->del(phy);
    }
    return ret;
}
#endif

/**
 * @brief 以太网驱动初始化函数
 * 
 * 根据配置初始化一个或多个以太网接口，支持内部以太网和SPI以太网的任意组合。
 * 主要操作：
 * 1. 参数有效性检查
 * 2. 分配以太网句柄数组内存
 * 3. 初始化内部以太网（如果启用）
 * 4. 初始化SPI总线和SPI以太网模块（如果启用）
 * 5. 为SPI以太网模块生成本地管理MAC地址
 * 
 * @param[out] eth_handles_out 输出参数，指向以太网驱动句柄数组的指针
 * @param[out] eth_cnt_out 输出参数，返回初始化的以太网接口数量
 * 
 * @return 
 *          - ESP_OK 初始化成功
 *          - ESP_ERR_INVALID_ARG 参数无效
 *          - ESP_ERR_NO_MEM 内存分配失败
 *          - ESP_FAIL 其他初始化失败
 */
esp_err_t example_eth_init(esp_eth_handle_t *eth_handles_out[], uint8_t *eth_cnt_out)
{
    esp_err_t ret = ESP_OK;
    esp_eth_handle_t *eth_handles = NULL;
    uint8_t eth_cnt = 0;

#if CONFIG_EXAMPLE_USE_INTERNAL_ETHERNET || CONFIG_EXAMPLE_USE_SPI_ETHERNET
    ESP_GOTO_ON_FALSE(eth_handles_out != NULL && eth_cnt_out != NULL, ESP_ERR_INVALID_ARG,
                        err, TAG, "invalid arguments: initialized handles array or number of interfaces");
    eth_handles = calloc(SPI_ETHERNETS_NUM + INTERNAL_ETHERNETS_NUM, sizeof(esp_eth_handle_t));
    ESP_GOTO_ON_FALSE(eth_handles != NULL, ESP_ERR_NO_MEM, err, TAG, "no memory");

#if CONFIG_EXAMPLE_USE_INTERNAL_ETHERNET
    eth_handles[eth_cnt] = eth_init_internal(NULL, NULL);
    ESP_GOTO_ON_FALSE(eth_handles[eth_cnt], ESP_FAIL, err, TAG, "internal Ethernet init failed");
    eth_cnt++;
#endif

#if CONFIG_EXAMPLE_USE_SPI_ETHERNET
    ESP_GOTO_ON_ERROR(spi_bus_init(), err, TAG, "SPI bus init failed");
    spi_eth_module_config_t spi_eth_module_config[CONFIG_EXAMPLE_SPI_ETHERNETS_NUM] = { 0 };
    INIT_SPI_ETH_MODULE_CONFIG(spi_eth_module_config, 0);

    uint8_t base_mac_addr[ETH_ADDR_LEN];
    ESP_GOTO_ON_ERROR(esp_efuse_mac_get_default(base_mac_addr), err, TAG, "get EFUSE MAC failed");
    uint8_t local_mac_1[ETH_ADDR_LEN];
    esp_derive_local_mac(local_mac_1, base_mac_addr);
    spi_eth_module_config[0].mac_addr = local_mac_1;

#if CONFIG_EXAMPLE_SPI_ETHERNETS_NUM > 1
    INIT_SPI_ETH_MODULE_CONFIG(spi_eth_module_config, 1);
    uint8_t local_mac_2[ETH_ADDR_LEN];
    base_mac_addr[ETH_ADDR_LEN - 1] += 1;
    esp_derive_local_mac(local_mac_2, base_mac_addr);
    spi_eth_module_config[1].mac_addr = local_mac_2;
#endif

#if CONFIG_EXAMPLE_SPI_ETHERNETS_NUM > 2
#error Maximum number of supported SPI Ethernet devices is currently limited to 2 by this example.
#endif

    for (int i = 0; i < CONFIG_EXAMPLE_SPI_ETHERNETS_NUM; i++) {
        eth_handles[eth_cnt] = eth_init_spi(&spi_eth_module_config[i], NULL, NULL);
        ESP_GOTO_ON_FALSE(eth_handles[eth_cnt], ESP_FAIL, err, TAG, "SPI Ethernet init failed");
        eth_cnt++;
    }
#endif
#else
    ESP_LOGD(TAG, "no Ethernet device selected to init");
#endif

    *eth_handles_out = eth_handles;
    *eth_cnt_out = eth_cnt;

    return ret;

#if CONFIG_EXAMPLE_USE_INTERNAL_ETHERNET || CONFIG_EXAMPLE_USE_SPI_ETHERNET
err:
    free(eth_handles);
    return ret;
#endif
}

/**
 * @brief 以太网驱动反初始化函数
 * 
 * 释放以太网驱动相关资源，执行以下操作：
 * 1. 参数有效性检查
 * 2. 遍历所有以太网接口，卸载驱动并释放MAC和PHY实例
 * 3. 释放SPI总线资源（如果使用SPI以太网）
 * 4. 卸载GPIO ISR服务（如果由本组件安装）
 * 5. 释放以太网句柄数组内存
 * 
 * @note 调用此函数前，所有以太网驱动必须已停止
 * 
 * @param[in] eth_handles 以太网驱动句柄数组
 * @param[in] eth_cnt 以太网接口数量
 * 
 * @return 
 *          - ESP_OK 反初始化成功
 *          - ESP_ERR_INVALID_ARG 参数无效
 */
esp_err_t example_eth_deinit(esp_eth_handle_t *eth_handles, uint8_t eth_cnt)
{
    ESP_RETURN_ON_FALSE(eth_handles != NULL, ESP_ERR_INVALID_ARG, TAG, "array of Ethernet handles cannot be NULL");

    for (int i = 0; i < eth_cnt; i++) {
        esp_eth_mac_t *mac = NULL;
        esp_eth_phy_t *phy = NULL;
        if (eth_handles[i] != NULL) {
            esp_eth_get_mac_instance(eth_handles[i], &mac);
            esp_eth_get_phy_instance(eth_handles[i], &phy);
            ESP_RETURN_ON_ERROR(esp_eth_driver_uninstall(eth_handles[i]), TAG, "Ethernet %p uninstall failed", eth_handles[i]);
        }
        if (mac != NULL) {
            mac->del(mac);
        }
        if (phy != NULL) {
            phy->del(phy);
        }
    }

#if CONFIG_EXAMPLE_USE_SPI_ETHERNET
    spi_bus_free(CONFIG_EXAMPLE_ETH_SPI_HOST);
#if (CONFIG_EXAMPLE_ETH_SPI_INT0_GPIO >= 0) || (CONFIG_EXAMPLE_ETH_SPI_INT1_GPIO > 0)
    if (gpio_isr_svc_init_by_eth) {
        ESP_LOGW(TAG, "uninstalling GPIO ISR service!");
        gpio_uninstall_isr_service();
    }
#endif
#endif

    free(eth_handles);
    return ESP_OK;
}
