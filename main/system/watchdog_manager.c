#include "watchdog_manager.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "WATCHDOG";

static bool initialized = false;
static bool running = false;
static uint32_t timeout_ms = WATCHDOG_DEFAULT_TIMEOUT_MS;
static watchdog_timeout_cb_t timeout_cb = NULL;
static TaskHandle_t feed_task = NULL;

static void watchdog_feed_task(void *arg)
{
    uint32_t feed_interval_ms = timeout_ms / 2;
    
    while (running) {
        esp_task_wdt_reset();
        ESP_LOGD(TAG, "Watchdog fed");
        vTaskDelay(pdMS_TO_TICKS(feed_interval_ms));
    }
    
    vTaskDelete(NULL);
}

esp_err_t watchdog_manager_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Watchdog already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing hardware watchdog...");
    
    esp_task_wdt_config_t config = {
        .timeout_ms = timeout_ms,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    
    esp_err_t ret = esp_task_wdt_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize watchdog: %d", ret);
        return ret;
    }
    
    ret = esp_task_wdt_add(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add current task to watchdog: %d", ret);
        esp_task_wdt_deinit();
        return ret;
    }
    
    initialized = true;
    ESP_LOGI(TAG, "Hardware watchdog initialized");
    return ESP_OK;
}

esp_err_t watchdog_manager_start(uint32_t timeout_ms_param)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Watchdog not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (running) {
        ESP_LOGW(TAG, "Watchdog already running");
        return ESP_OK;
    }
    
    if (timeout_ms_param < WATCHDOG_MIN_TIMEOUT_MS || 
        timeout_ms_param > WATCHDOG_MAX_TIMEOUT_MS) {
        ESP_LOGE(TAG, "Invalid timeout: %lu ms", timeout_ms_param);
        return ESP_ERR_INVALID_ARG;
    }
    
    timeout_ms = timeout_ms_param;
    
    esp_task_wdt_config_t config = {
        .timeout_ms = timeout_ms,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    
    esp_err_t ret = esp_task_wdt_reconfigure(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reconfigure watchdog: %d", ret);
        return ret;
    }
    
    running = true;
    
    BaseType_t task_ret = xTaskCreate(watchdog_feed_task, "wd_feed", 2048, 
                                      NULL, 15, &feed_task);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create feed task");
        running = false;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Hardware watchdog started with timeout: %lu ms", timeout_ms);
    return ESP_OK;
}

void watchdog_manager_stop(void)
{
    if (!running) {
        ESP_LOGW(TAG, "Watchdog not running");
        return;
    }
    
    running = false;
    
    if (feed_task) {
        vTaskDelete(feed_task);
        feed_task = NULL;
    }
    
    esp_task_wdt_delete(NULL);
    esp_task_wdt_deinit();
    
    ESP_LOGI(TAG, "Hardware watchdog stopped");
}

void watchdog_manager_feed(void)
{
    if (!running) {
        ESP_LOGW(TAG, "Watchdog not running");
        return;
    }
    
    esp_task_wdt_reset();
}

void watchdog_manager_pause(void)
{
    if (!running) {
        ESP_LOGW(TAG, "Watchdog not running");
        return;
    }
    
    esp_task_wdt_reset();
    ESP_LOGI(TAG, "Watchdog paused");
}

void watchdog_manager_resume(void)
{
    if (!running) {
        ESP_LOGW(TAG, "Watchdog not running");
        return;
    }
    
    esp_task_wdt_reset();
    ESP_LOGI(TAG, "Watchdog resumed");
}

watchdog_mode_t watchdog_manager_get_mode(void)
{
    if (!initialized) {
        return WATCHDOG_MODE_IDLE;
    }
    
    if (!running) {
        return WATCHDOG_MODE_IDLE;
    }
    
    return WATCHDOG_MODE_ACTIVE;
}

void watchdog_manager_register_timeout_callback(watchdog_timeout_cb_t cb)
{
    timeout_cb = cb;
    ESP_LOGI(TAG, "Watchdog timeout callback registered");
}

uint32_t watchdog_manager_get_timeout_ms(void)
{
    return timeout_ms;
}

bool watchdog_manager_is_running(void)
{
    return running;
}