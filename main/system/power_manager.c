#include "power_manager.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "bsp/display.h"

static const char *TAG = "POWER_MANAGER";

#define POWER_MONITOR_STACK_SIZE 2048

#define AUTO_SLEEP_DEFAULT_TIMEOUT_S 60
#define AUTO_SLEEP_MIN_TIMEOUT_S 15
#define AUTO_SLEEP_MAX_TIMEOUT_S 7200

static power_info_t s_power_info = {0};
static power_mode_t s_power_mode = POWER_MODE_NORMAL;
static power_event_cb_t s_event_cb = NULL;
static SemaphoreHandle_t s_power_mutex = NULL;
static TaskHandle_t s_auto_sleep_task = NULL;
static esp_pm_lock_handle_t s_cpu_lock = NULL;
static esp_pm_lock_handle_t s_apb_lock = NULL;
static int s_auto_sleep_timeout = AUTO_SLEEP_DEFAULT_TIMEOUT_S;
static bool s_auto_sleep_enabled = true;
static volatile bool s_activity_detected = true;

static void enter_deep_sleep(void)
{
    ESP_LOGI(TAG, "Entering deep sleep...");
    
    bsp_display_backlight_off();
    
    esp_err_t ret = esp_sleep_enable_ext1_wakeup_io((1ULL << GPIO_NUM_0), ESP_EXT1_WAKEUP_ANY_LOW);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable ext1 wakeup: %s", esp_err_to_name(ret));
    }
    
    esp_deep_sleep_start();
}

static void auto_sleep_task(void *arg)
{
    int idle_count = 0;
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        if (!s_auto_sleep_enabled || s_power_mode == POWER_MODE_SLEEP) {
            idle_count = 0;
            continue;
        }
        
        if (s_activity_detected) {
            idle_count = 0;
            s_activity_detected = false;
            continue;
        }
        
        idle_count++;
        
        if (idle_count >= s_auto_sleep_timeout) {
            ESP_LOGI(TAG, "Auto sleep triggered after %d seconds idle", s_auto_sleep_timeout);
            power_manager_set_mode(POWER_MODE_SLEEP);
        }
    }
}

esp_err_t power_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing power manager (direct power mode)...");
    
    s_power_mutex = xSemaphoreCreateMutex();
    if (!s_power_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    esp_pm_config_t pm_config = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = 20,
        .light_sleep_enable = true,
    };
    esp_err_t ret = esp_pm_configure(&pm_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure power management: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "cpu_lock", &s_cpu_lock);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create CPU lock: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_pm_lock_create(ESP_PM_APB_FREQ_MAX, 0, "apb_lock", &s_apb_lock);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create APB lock: %s", esp_err_to_name(ret));
        return ret;
    }
    
    s_power_info.state = POWER_STATE_AC_POWERED;
    s_power_info.battery_level = -1;
    s_power_info.voltage = 0;
    s_power_info.ac_present = true;
    
    ESP_LOGI(TAG, "Auto sleep timeout: %ds (min:%ds, max:%ds)", 
             s_auto_sleep_timeout, AUTO_SLEEP_MIN_TIMEOUT_S, AUTO_SLEEP_MAX_TIMEOUT_S);
    
    BaseType_t task_ret = xTaskCreate(auto_sleep_task, "auto_sleep", 
                                     POWER_MONITOR_STACK_SIZE, NULL, 3, &s_auto_sleep_task);
    if (task_ret != pdPASS) {
        ESP_LOGW(TAG, "Failed to create auto sleep task");
    }
    
    ESP_LOGI(TAG, "Power manager initialized");
    return ESP_OK;
}

void power_manager_get_info(power_info_t *info)
{
    if (!info) return;
    
    if (xSemaphoreTake(s_power_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    
    *info = s_power_info;
    xSemaphoreGive(s_power_mutex);
}

power_mode_t power_manager_get_mode(void)
{
    return s_power_mode;
}

esp_err_t power_manager_set_mode(power_mode_t mode)
{
    if (xSemaphoreTake(s_power_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    
    s_power_mode = mode;
    
    switch (mode) {
        case POWER_MODE_NORMAL:
            ESP_LOGI(TAG, "Setting normal power mode");
            esp_pm_lock_acquire(s_cpu_lock);
            esp_pm_lock_acquire(s_apb_lock);
            bsp_display_backlight_on();
            break;
            
        case POWER_MODE_LOW_POWER:
            ESP_LOGI(TAG, "Setting low power mode");
            esp_pm_lock_release(s_cpu_lock);
            esp_pm_lock_release(s_apb_lock);
            break;
            
        case POWER_MODE_SLEEP:
            ESP_LOGI(TAG, "Setting sleep mode");
            esp_pm_lock_release(s_cpu_lock);
            esp_pm_lock_release(s_apb_lock);
            xSemaphoreGive(s_power_mutex);
            enter_deep_sleep();
            return ESP_OK;
            
        default:
            xSemaphoreGive(s_power_mutex);
            return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreGive(s_power_mutex);
    return ESP_OK;
}

void power_manager_register_event_cb(power_event_cb_t cb)
{
    s_event_cb = cb;
}

int power_manager_get_battery_level(void)
{
    return -1;
}

bool power_manager_is_charging(void)
{
    return true;
}

void power_manager_reset_activity_timer(void)
{
    s_activity_detected = true;
}

void power_manager_set_auto_sleep_timeout(int seconds)
{
    if (seconds < AUTO_SLEEP_MIN_TIMEOUT_S) {
        seconds = AUTO_SLEEP_MIN_TIMEOUT_S;
    } else if (seconds > AUTO_SLEEP_MAX_TIMEOUT_S) {
        seconds = AUTO_SLEEP_MAX_TIMEOUT_S;
    }
    s_auto_sleep_timeout = seconds;
}

int power_manager_get_auto_sleep_timeout(void)
{
    return s_auto_sleep_timeout;
}

void power_manager_set_auto_sleep_enabled(bool enabled)
{
    s_auto_sleep_enabled = enabled;
}

bool power_manager_is_auto_sleep_enabled(void)
{
    return s_auto_sleep_enabled;
}