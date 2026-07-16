#include "crash_handler.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_vfs.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "CRASH_HANDLER";

static void (*crash_cb)(crash_info_t *info) = NULL;
static bool watchdog_enabled = false;
static uint32_t watchdog_timeout_ms = 0;
static uint32_t last_watchdog_feed = 0;

static void crash_handler_task(void *arg)
{
    while (watchdog_enabled) {
        uint32_t current_time = esp_timer_get_time() / 1000;
        
        if (current_time - last_watchdog_feed > watchdog_timeout_ms) {
            ESP_LOGE(TAG, "Watchdog timeout! System will reboot.");
            crash_handler_save_log(CRASH_TYPE_WATCHDOG, "Watchdog timeout");
            crash_handler_reboot();
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    vTaskDelete(NULL);
}

esp_err_t crash_handler_init(void)
{
    ESP_LOGI(TAG, "Initializing crash handler...");
    
    last_watchdog_feed = esp_timer_get_time() / 1000;
    
    ESP_LOGI(TAG, "Crash handler initialized");
    return ESP_OK;
}

void crash_handler_register_callback(void (*cb)(crash_info_t *info))
{
    crash_cb = cb;
    ESP_LOGI(TAG, "Crash callback registered");
}

void crash_handler_save_log(crash_type_t type, const char *message)
{
    FILE *file = fopen(CRASH_LOG_FILE, "w");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open crash log file");
        return;
    }
    
    uint32_t timestamp = esp_timer_get_time() / 1000;
    
    const char *type_str = "";
    switch (type) {
        case CRASH_TYPE_ASSERT: type_str = "ASSERT"; break;
        case CRASH_TYPE_WATCHDOG: type_str = "WATCHDOG"; break;
        case CRASH_TYPE_EXCEPTION: type_str = "EXCEPTION"; break;
        case CRASH_TYPE_STACK_OVERFLOW: type_str = "STACK_OVERFLOW"; break;
        default: type_str = "UNKNOWN"; break;
    }
    
    fprintf(file, "=== CRASH LOG ===\n");
    fprintf(file, "Type: %s\n", type_str);
    fprintf(file, "Timestamp: %lu ms\n", timestamp);
    fprintf(file, "Message: %s\n", message ? message : "none");
    
    fclose(file);
    
    ESP_LOGI(TAG, "Crash log saved: %s - %s", type_str, message ? message : "none");
    
    if (crash_cb) {
        crash_info_t info = {
            .type = type,
            .timestamp = timestamp,
            .message = {0}
        };
        if (message) {
            strncpy(info.message, message, sizeof(info.message) - 1);
        }
        crash_cb(&info);
    }
}

void crash_handler_save_log_with_context(crash_type_t type, const char *message, 
                                         uint32_t pc, uint32_t sp, uint32_t ps)
{
    FILE *file = fopen(CRASH_LOG_FILE, "w");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open crash log file");
        return;
    }
    
    uint32_t timestamp = esp_timer_get_time() / 1000;
    
    const char *type_str = "";
    switch (type) {
        case CRASH_TYPE_ASSERT: type_str = "ASSERT"; break;
        case CRASH_TYPE_WATCHDOG: type_str = "WATCHDOG"; break;
        case CRASH_TYPE_EXCEPTION: type_str = "EXCEPTION"; break;
        case CRASH_TYPE_STACK_OVERFLOW: type_str = "STACK_OVERFLOW"; break;
        default: type_str = "UNKNOWN"; break;
    }
    
    fprintf(file, "=== CRASH LOG ===\n");
    fprintf(file, "Type: %s\n", type_str);
    fprintf(file, "Timestamp: %lu ms\n", timestamp);
    fprintf(file, "PC: 0x%08lx\n", pc);
    fprintf(file, "SP: 0x%08lx\n", sp);
    fprintf(file, "PS: 0x%08lx\n", ps);
    fprintf(file, "Message: %s\n", message ? message : "none");
    
    fclose(file);
    
    ESP_LOGI(TAG, "Crash log saved with context: %s - PC=0x%08lx", type_str, pc);
    
    if (crash_cb) {
        crash_info_t info = {
            .type = type,
            .timestamp = timestamp,
            .pc = pc,
            .sp = sp,
            .ps = ps,
            .message = {0}
        };
        if (message) {
            strncpy(info.message, message, sizeof(info.message) - 1);
        }
        crash_cb(&info);
    }
}

bool crash_handler_has_crash_log(void)
{
    FILE *file = fopen(CRASH_LOG_FILE, "r");
    if (!file) {
        return false;
    }
    fclose(file);
    return true;
}

esp_err_t crash_handler_read_log(crash_info_t *info)
{
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    FILE *file = fopen(CRASH_LOG_FILE, "r");
    if (!file) {
        return ESP_ERR_NOT_FOUND;
    }
    
    char buffer[CRASH_LOG_SIZE];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[bytes_read] = '\0';
    fclose(file);
    
    memset(info, 0, sizeof(crash_info_t));
    strncpy(info->stack_trace, buffer, sizeof(info->stack_trace) - 1);
    
    return ESP_OK;
}

esp_err_t crash_handler_clear_log(void)
{
    if (remove(CRASH_LOG_FILE) == 0) {
        ESP_LOGI(TAG, "Crash log cleared");
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

void crash_handler_reboot(void)
{
    ESP_LOGI(TAG, "System rebooting...");
    
    for (int i = 5; i > 0; i--) {
        ESP_LOGI(TAG, "Rebooting in %d seconds...", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    esp_restart();
}

void crash_handler_watchdog_enable(uint32_t timeout_ms)
{
    if (watchdog_enabled) {
        ESP_LOGW(TAG, "Watchdog already enabled");
        return;
    }
    
    watchdog_enabled = true;
    watchdog_timeout_ms = timeout_ms;
    last_watchdog_feed = esp_timer_get_time() / 1000;
    
    xTaskCreate(crash_handler_task, "crash_watchdog", 4096, NULL, 10, NULL);
    
    ESP_LOGI(TAG, "Watchdog enabled with timeout: %lu ms", timeout_ms);
}

void crash_handler_watchdog_disable(void)
{
    watchdog_enabled = false;
    ESP_LOGI(TAG, "Watchdog disabled");
}

void crash_handler_watchdog_feed(void)
{
    last_watchdog_feed = esp_timer_get_time() / 1000;
}