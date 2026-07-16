/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/**
 * @file ethernet_example_main.c
 * @brief Ethernet to Wi-Fi AP数据包转发示例
 * 
 * 本示例实现了以太网到Wi-Fi AP的数据包双向转发功能，将ESP32作为一个桥接器：
 * 1. 接收以太网数据包并转发到Wi-Fi AP
 * 2. 接收Wi-Fi AP数据包并转发到以太网
 * 3. 使用队列进行流量控制，平衡以太网和Wi-Fi之间的速度差异
 * 
 * 由于以太网速度快于Wi-Fi，需要使用队列进行流量控制，防止数据包丢失。
 */

#include <string.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_eth_driver.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_private/wifi.h"
#include "ethernet_init.h"

static const char *TAG = "eth2ap_example";
static esp_eth_handle_t s_eth_handle = NULL;
static QueueHandle_t flow_control_queue = NULL;
static bool s_sta_is_connected = false;
static bool s_ethernet_is_connected = false;
static uint8_t s_eth_mac[6];

#define FLOW_CONTROL_QUEUE_TIMEOUT_MS (100)
#define FLOW_CONTROL_QUEUE_LENGTH (40)
#define FLOW_CONTROL_WIFI_SEND_TIMEOUT_MS (100)

/**
 * @brief 流量控制消息结构体
 * 
 * 用于在以太网接收回调和Wi-Fi发送任务之间传递数据包。
 */
typedef struct {
    void *packet;    /**< 数据包指针 */
    uint16_t length; /**< 数据包长度 */
} flow_control_msg_t;

/**
 * @brief Wi-Fi到以太网数据包转发函数
 * 
 * 当Wi-Fi AP接收到数据包时，将其转发到以太网接口。
 * 如果以太网未连接，则释放数据包。
 * 
 * @param buffer 数据包缓冲区指针
 * @param len 数据包长度
 * @param eb Wi-Fi接收缓冲区句柄
 * 
 * @return esp_err_t ESP_OK表示成功
 */
static esp_err_t pkt_wifi2eth(void *buffer, uint16_t len, void *eb)
{
    if (s_ethernet_is_connected) {
        if (esp_eth_transmit(s_eth_handle, buffer, len) != ESP_OK) {
            ESP_LOGE(TAG, "Ethernet send packet failed");
        }
    }
    esp_wifi_internal_free_rx_buffer(eb);
    return ESP_OK;
}

/**
 * @brief 以太网到Wi-Fi数据包转发函数
 * 
 * 当以太网接收到数据包时，将其放入流量控制队列。
 * 由于以太网速度快于Wi-Fi，需要使用队列平衡速度差异。
 * 
 * @param eth_handle 以太网驱动句柄
 * @param buffer 数据包缓冲区指针
 * @param len 数据包长度
 * @param priv 用户私有数据（未使用）
 * 
 * @return esp_err_t ESP_OK表示成功，ESP_FAIL表示队列满
 */
static esp_err_t pkt_eth2wifi(esp_eth_handle_t eth_handle, uint8_t *buffer, uint32_t len, void *priv)
{
    esp_err_t ret = ESP_OK;
    flow_control_msg_t msg = {
        .packet = buffer,
        .length = len
    };
    if (xQueueSend(flow_control_queue, &msg, pdMS_TO_TICKS(FLOW_CONTROL_QUEUE_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "send flow control message failed or timeout");
        free(buffer);
        ret = ESP_FAIL;
    }
    return ret;
}

/**
 * @brief 以太网到Wi-Fi流量控制任务
 * 
 * 从队列中取出数据包，并通过Wi-Fi AP发送出去。
 * 如果Wi-Fi发送失败，会进行重试，最多等待FLOW_CONTROL_WIFI_SEND_TIMEOUT_MS毫秒。
 * 
 * @param args 任务参数（未使用）
 */
static void eth2wifi_flow_control_task(void *args)
{
    flow_control_msg_t msg;
    int res = 0;
    uint32_t timeout = 0;
    while (1) {
        if (xQueueReceive(flow_control_queue, &msg, pdMS_TO_TICKS(FLOW_CONTROL_QUEUE_TIMEOUT_MS)) == pdTRUE) {
            timeout = 0;
            if (s_sta_is_connected && msg.length) {
                do {
                    vTaskDelay(pdMS_TO_TICKS(timeout));
                    timeout += 2;
                    res = esp_wifi_internal_tx(WIFI_IF_AP, msg.packet, msg.length);
                } while (res && timeout < FLOW_CONTROL_WIFI_SEND_TIMEOUT_MS);
                if (res != ESP_OK) {
                    ESP_LOGE(TAG, "WiFi send packet failed: %d", res);
                }
            }
            free(msg.packet);
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief 以太网事件处理函数
 * 
 * 处理以太网相关事件：
 * 1. ETHERNET_EVENT_CONNECTED: 以太网连接成功，设置MAC地址到Wi-Fi AP并启动Wi-Fi
 * 2. ETHERNET_EVENT_DISCONNECTED: 以太网断开，停止Wi-Fi
 * 3. ETHERNET_EVENT_START: 以太网启动成功
 * 4. ETHERNET_EVENT_STOP: 以太网停止
 * 
 * @param arg 用户参数（未使用）
 * @param event_base 事件基（ETH_EVENT）
 * @param event_id 事件ID
 * @param event_data 事件数据
 */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Up");
        s_ethernet_is_connected = true;
        esp_eth_ioctl(s_eth_handle, ETH_CMD_G_MAC_ADDR, s_eth_mac);
        esp_wifi_set_mac(WIFI_IF_AP, s_eth_mac);
        ESP_ERROR_CHECK(esp_wifi_start());
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        s_ethernet_is_connected = false;
        ESP_ERROR_CHECK(esp_wifi_stop());
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/**
 * @brief Wi-Fi事件处理函数
 * 
 * 处理Wi-Fi AP相关事件：
 * 1. WIFI_EVENT_AP_STACONNECTED: 有站点连接到Wi-Fi AP，注册数据包接收回调
 * 2. WIFI_EVENT_AP_STADISCONNECTED: 站点断开连接，取消数据包接收回调
 * 
 * @param arg 用户参数（未使用）
 * @param event_base 事件基（WIFI_EVENT）
 * @param event_id 事件ID
 * @param event_data 事件数据
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    static uint8_t s_con_cnt = 0;
    switch (event_id) {
    case WIFI_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "Wi-Fi AP got a station connected");
        if (!s_con_cnt) {
            s_sta_is_connected = true;
            esp_wifi_internal_reg_rxcb(WIFI_IF_AP, pkt_wifi2eth);
        }
        s_con_cnt++;
        break;
    case WIFI_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "Wi-Fi AP got a station disconnected");
        s_con_cnt--;
        if (!s_con_cnt) {
            s_sta_is_connected = false;
            esp_wifi_internal_reg_rxcb(WIFI_IF_AP, NULL);
        }
        break;
    default:
        break;
    }
}

/**
 * @brief 初始化以太网接口
 * 
 * 初始化以太网驱动，配置数据包接收回调，设置混杂模式，并启动以太网。
 */
static void initialize_ethernet(void)
{
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    ESP_ERROR_CHECK(example_eth_init(&eth_handles, &eth_port_cnt));
    if (eth_port_cnt > 1) {
        ESP_LOGW(TAG, "multiple Ethernet devices detected, the first initialized is to be used!");
    }
    s_eth_handle = eth_handles[0];
    free(eth_handles);
    ESP_ERROR_CHECK(esp_eth_update_input_path(s_eth_handle, pkt_eth2wifi, NULL));
    bool eth_promiscuous = true;
    ESP_ERROR_CHECK(esp_eth_ioctl(s_eth_handle, ETH_CMD_S_PROMISCUOUS, &eth_promiscuous));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));
}

/**
 * @brief 初始化Wi-Fi AP接口
 * 
 * 初始化Wi-Fi驱动，配置AP模式参数（SSID、密码、最大连接数、认证模式、信道），
 * 设置Wi-Fi存储方式为RAM。
 */
static void initialize_wifi(void)
{
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_EXAMPLE_WIFI_SSID,
            .ssid_len = strlen(CONFIG_EXAMPLE_WIFI_SSID),
            .password = CONFIG_EXAMPLE_WIFI_PASSWORD,
            .max_connection = CONFIG_EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .channel = CONFIG_EXAMPLE_WIFI_CHANNEL
        },
    };
    if (strlen(CONFIG_EXAMPLE_WIFI_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
}

/**
 * @brief 初始化流量控制队列和任务
 * 
 * 创建流量控制队列和流量控制任务，用于平衡以太网和Wi-Fi之间的速度差异。
 * 
 * @return 
 *          - ESP_OK 初始化成功
 *          - ESP_FAIL 队列或任务创建失败
 */
static esp_err_t initialize_flow_control(void)
{
    flow_control_queue = xQueueCreate(FLOW_CONTROL_QUEUE_LENGTH, sizeof(flow_control_msg_t));
    if (!flow_control_queue) {
        ESP_LOGE(TAG, "create flow control queue failed");
        return ESP_FAIL;
    }
    BaseType_t ret = xTaskCreate(eth2wifi_flow_control_task, "flow_ctl", 2048, NULL, (tskIDLE_PRIORITY + 2), NULL);
    if (ret != pdTRUE) {
        ESP_LOGE(TAG, "create flow control task failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief ESP-IDF应用程序入口函数
 * 
 * 主函数负责初始化整个系统：
 * 1. 初始化NVS Flash
 * 2. 创建默认事件循环
 * 3. 初始化流量控制队列和任务
 * 4. 初始化Wi-Fi AP接口
 * 5. 初始化以太网接口
 * 
 * @return void 无返回值
 */
void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(initialize_flow_control());
    initialize_wifi();
    initialize_ethernet();
}
