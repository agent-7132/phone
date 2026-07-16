#include "service_manager.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "SERVICE_MANAGER";

static service_info_t services[SERVICE_MANAGER_MAX_SERVICES];
static int service_count = 0;
static service_event_cb_t event_cb = NULL;
static void *event_cb_arg = NULL;
static SemaphoreHandle_t service_mutex = NULL;

static int find_service_index(const char *name)
{
    for (int i = 0; i < service_count; i++) {
        if (strcmp(services[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void notify_event(const char *name, service_state_t state);
static void service_crash_handler(service_info_t *service);

static void notify_event(const char *name, service_state_t state)
{
    if (event_cb) {
        event_cb(name, state, event_cb_arg);
    }
}

static void service_task_wrapper(void *arg)
{
    service_info_t *service = (service_info_t *)arg;
    
    ESP_LOGI(TAG, "Service task started: %s", service->name);
    service->state = SERVICE_STATE_RUNNING;
    service->start_time = esp_timer_get_time();
    notify_event(service->name, SERVICE_STATE_RUNNING);
    
    if (service->start) {
        esp_err_t ret = service->start(service->arg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Service %s start failed: %s", service->name, esp_err_to_name(ret));
            service_crash_handler(service);
            vTaskDelete(NULL);
            return;
        }
    }
    
    service->state = SERVICE_STATE_STOPPED;
    service->total_run_time += esp_timer_get_time() - service->start_time;
    ESP_LOGI(TAG, "Service task stopped: %s", service->name);
    notify_event(service->name, SERVICE_STATE_STOPPED);
    
    vTaskDelete(NULL);
}

static void service_crash_handler(service_info_t *service)
{
    service->state = SERVICE_STATE_CRASHED;
    service->restart_count++;
    service->total_run_time += esp_timer_get_time() - service->start_time;
    ESP_LOGE(TAG, "Service crashed: %s (count=%d)", service->name, service->restart_count);
    notify_event(service->name, SERVICE_STATE_CRASHED);
    
    if (service->auto_restart && service->restart_count < service->max_restarts) {
        ESP_LOGI(TAG, "Auto-restarting service %s in %lu ms", service->name, service->restart_delay_ms);
        vTaskDelay(pdMS_TO_TICKS(service->restart_delay_ms));
        service_manager_start(service->name);
    }
}

esp_err_t service_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing service manager...");
    
    service_count = 0;
    memset(services, 0, sizeof(services));
    event_cb = NULL;
    event_cb_arg = NULL;
    
    service_mutex = xSemaphoreCreateRecursiveMutex();
    if (!service_mutex) {
        ESP_LOGE(TAG, "Failed to create service mutex");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Service manager initialized");
    return ESP_OK;
}

esp_err_t service_manager_register(const char *name, 
                                   esp_err_t (*start)(void *arg),
                                   void (*stop)(void *arg),
                                   void (*pause)(void *arg),
                                   void (*resume)(void *arg),
                                   bool (*health_check)(void *arg),
                                   service_priority_t priority, 
                                   size_t stack_size,
                                   void *arg)
{
    if (!name || !start) {
        ESP_LOGE(TAG, "Invalid service parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    
    if (service_count >= SERVICE_MANAGER_MAX_SERVICES) {
        ESP_LOGE(TAG, "Maximum services reached");
        xSemaphoreGiveRecursive(service_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    if (find_service_index(name) >= 0) {
        ESP_LOGE(TAG, "Service already exists: %s", name);
        xSemaphoreGiveRecursive(service_mutex);
        return ESP_ERR_INVALID_ARG;
    }
    
    service_info_t *service = &services[service_count];
    memset(service, 0, sizeof(service_info_t));
    strncpy(service->name, name, sizeof(service->name) - 1);
    service->name[sizeof(service->name) - 1] = '\0';
    service->start = start;
    service->stop = stop;
    service->pause = pause;
    service->resume = resume;
    service->health_check = health_check;
    service->arg = arg;
    service->state = SERVICE_STATE_STOPPED;
    service->priority = priority;
    service->stack_size = stack_size;
    
    service->auto_restart = false;
    service->restart_count = 0;
    service->max_restarts = 3;
    service->restart_delay_ms = 5000;
    
    service->max_memory_bytes = 0;
    service->cpu_usage_percent = 0;
    service->health_check_interval_ms = 0;
    service->last_health_check = esp_timer_get_time();
    
    service->dependency_count = 0;
    
    service_count++;
    
    ESP_LOGI(TAG, "Registered service: %s (priority=%d, stack=%u)", name, priority, stack_size);
    
    xSemaphoreGiveRecursive(service_mutex);
    return ESP_OK;
}

esp_err_t service_manager_unregister(const char *name)
{
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    
    int index = find_service_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Service not found: %s", name);
        xSemaphoreGiveRecursive(service_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    service_info_t *service = &services[index];
    
    if (service->state == SERVICE_STATE_RUNNING) {
        xSemaphoreGiveRecursive(service_mutex);
        service_manager_stop(name);
        xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
        index = find_service_index(name);
        if (index < 0) {
            xSemaphoreGiveRecursive(service_mutex);
            return ESP_OK;
        }
    }
    
    ESP_LOGI(TAG, "Unregistered service: %s", name);
    
    for (int i = index; i < service_count - 1; i++) {
        services[i] = services[i + 1];
    }
    service_count--;
    
    xSemaphoreGiveRecursive(service_mutex);
    return ESP_OK;
}

esp_err_t service_manager_start(const char *name)
{
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    
    int index = find_service_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Service not found: %s", name);
        xSemaphoreGiveRecursive(service_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    service_info_t *service = &services[index];
    
    if (service->state == SERVICE_STATE_RUNNING) {
        ESP_LOGW(TAG, "Service already running: %s", name);
        xSemaphoreGiveRecursive(service_mutex);
        return ESP_OK;
    }
    
    for (int i = 0; i < service->dependency_count; i++) {
        service_state_t dep_state = service_manager_get_state(service->dependencies[i]);
        if (dep_state != SERVICE_STATE_RUNNING) {
            ESP_LOGW(TAG, "Dependency %s not running, starting it first", service->dependencies[i]);
            service_manager_start(service->dependencies[i]);
        }
    }
    
    service->state = SERVICE_STATE_STARTING;
    notify_event(service->name, SERVICE_STATE_STARTING);
    
    xSemaphoreGiveRecursive(service_mutex);
    
    BaseType_t ret = xTaskCreate(service_task_wrapper, name, service->stack_size, 
                                 service, service->priority, &service->task_handle);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create service task: %s", name);
        xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
        service->state = SERVICE_STATE_STOPPED;
        notify_event(service->name, SERVICE_STATE_STOPPED);
        xSemaphoreGiveRecursive(service_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Started service: %s", name);
    
    return ESP_OK;
}

esp_err_t service_manager_stop(const char *name)
{
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    
    int index = find_service_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Service not found: %s", name);
        xSemaphoreGiveRecursive(service_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    service_info_t *service = &services[index];
    
    if (service->state != SERVICE_STATE_RUNNING && service->state != SERVICE_STATE_PAUSED) {
        ESP_LOGW(TAG, "Service not running: %s", name);
        xSemaphoreGiveRecursive(service_mutex);
        return ESP_OK;
    }
    
    if (service->stop) {
        xSemaphoreGiveRecursive(service_mutex);
        service->stop(service->arg);
        xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    }
    
    if (service->task_handle) {
        vTaskDelete(service->task_handle);
        service->task_handle = NULL;
    }
    
    service->state = SERVICE_STATE_STOPPED;
    service->total_run_time += esp_timer_get_time() - service->start_time;
    ESP_LOGI(TAG, "Stopped service: %s", name);
    notify_event(service->name, SERVICE_STATE_STOPPED);
    
    xSemaphoreGiveRecursive(service_mutex);
    return ESP_OK;
}

esp_err_t service_manager_pause(const char *name)
{
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    
    int index = find_service_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Service not found: %s", name);
        xSemaphoreGiveRecursive(service_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    service_info_t *service = &services[index];
    
    if (service->state != SERVICE_STATE_RUNNING) {
        ESP_LOGW(TAG, "Service not running: %s", name);
        xSemaphoreGiveRecursive(service_mutex);
        return ESP_OK;
    }
    
    if (service->pause) {
        xSemaphoreGiveRecursive(service_mutex);
        service->pause(service->arg);
        xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    }
    
    if (service->task_handle) {
        vTaskSuspend(service->task_handle);
    }
    
    service->state = SERVICE_STATE_PAUSED;
    service->total_run_time += esp_timer_get_time() - service->start_time;
    ESP_LOGI(TAG, "Paused service: %s", name);
    notify_event(service->name, SERVICE_STATE_PAUSED);
    
    xSemaphoreGiveRecursive(service_mutex);
    return ESP_OK;
}

esp_err_t service_manager_resume(const char *name)
{
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    
    int index = find_service_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Service not found: %s", name);
        xSemaphoreGiveRecursive(service_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    service_info_t *service = &services[index];
    
    if (service->state != SERVICE_STATE_PAUSED) {
        ESP_LOGW(TAG, "Service not paused: %s", name);
        xSemaphoreGiveRecursive(service_mutex);
        return ESP_OK;
    }
    
    if (service->resume) {
        xSemaphoreGiveRecursive(service_mutex);
        service->resume(service->arg);
        xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    }
    
    if (service->task_handle) {
        vTaskResume(service->task_handle);
    }
    
    service->state = SERVICE_STATE_RUNNING;
    service->start_time = esp_timer_get_time();
    ESP_LOGI(TAG, "Resumed service: %s", name);
    notify_event(service->name, SERVICE_STATE_RUNNING);
    
    xSemaphoreGiveRecursive(service_mutex);
    return ESP_OK;
}

service_state_t service_manager_get_state(const char *name)
{
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    int index = find_service_index(name);
    service_state_t state = index < 0 ? SERVICE_STATE_STOPPED : services[index].state;
    xSemaphoreGiveRecursive(service_mutex);
    return state;
}

service_info_t *service_manager_get_service(const char *name)
{
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    int index = find_service_index(name);
    service_info_t *service = index < 0 ? NULL : &services[index];
    xSemaphoreGiveRecursive(service_mutex);
    return service;
}

service_info_t *service_manager_get_service_by_index(int index)
{
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    service_info_t *service = (index < 0 || index >= service_count) ? NULL : &services[index];
    xSemaphoreGiveRecursive(service_mutex);
    return service;
}

int service_manager_get_service_count(void)
{
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    int count = service_count;
    xSemaphoreGiveRecursive(service_mutex);
    return count;
}

void service_manager_dump_services(void)
{
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    
    ESP_LOGI(TAG, "=== Service Manager Dump ===");
    ESP_LOGI(TAG, "Total services: %d", service_count);
    
    for (int i = 0; i < service_count; i++) {
        const char *state_str = "";
        switch (services[i].state) {
            case SERVICE_STATE_STOPPED: state_str = "STOPPED"; break;
            case SERVICE_STATE_RUNNING: state_str = "RUNNING"; break;
            case SERVICE_STATE_PAUSED: state_str = "PAUSED"; break;
            case SERVICE_STATE_CRASHED: state_str = "CRASHED"; break;
            case SERVICE_STATE_STARTING: state_str = "STARTING"; break;
        }
        
        ESP_LOGI(TAG, "[%d] %s - %s (priority=%d, stack=%u)", 
                 i, services[i].name, state_str, services[i].priority, services[i].stack_size);
        
        if (services[i].auto_restart) {
            ESP_LOGI(TAG, "  Auto-restart: ON (max=%d, delay=%lu ms, current=%d)", 
                     services[i].max_restarts, services[i].restart_delay_ms, services[i].restart_count);
        }
        
        if (services[i].resource_limited) {
            ESP_LOGI(TAG, "  Resource limits: memory=%lu bytes, CPU=%lu%%", 
                     services[i].max_memory_bytes, services[i].cpu_usage_percent);
        }
        
        if (services[i].health_check_interval_ms > 0) {
            ESP_LOGI(TAG, "  Health check: interval=%lu ms", services[i].health_check_interval_ms);
        }
        
        if (services[i].dependency_count > 0) {
            ESP_LOGI(TAG, "  Dependencies:");
            for (int j = 0; j < services[i].dependency_count; j++) {
                ESP_LOGI(TAG, "    - %s", services[i].dependencies[j]);
            }
        }
    }
    
    ESP_LOGI(TAG, "=== End Dump ===");
    
    xSemaphoreGiveRecursive(service_mutex);
}

esp_err_t service_manager_add_dependency(const char *name, const char *dep_name)
{
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    
    int index = find_service_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Service not found: %s", name);
        xSemaphoreGiveRecursive(service_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    service_info_t *service = &services[index];
    
    if (service->dependency_count >= SERVICE_MANAGER_MAX_DEPENDENCIES) {
        ESP_LOGE(TAG, "Max dependencies reached for service: %s", name);
        xSemaphoreGiveRecursive(service_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    for (int i = 0; i < service->dependency_count; i++) {
        if (strcmp(service->dependencies[i], dep_name) == 0) {
            ESP_LOGW(TAG, "Dependency already exists: %s -> %s", name, dep_name);
            xSemaphoreGiveRecursive(service_mutex);
            return ESP_OK;
        }
    }
    
    strncpy(service->dependencies[service->dependency_count], dep_name, sizeof(service->dependencies[0]) - 1);
    service->dependency_count++;
    
    ESP_LOGI(TAG, "Added dependency: %s -> %s", name, dep_name);
    
    xSemaphoreGiveRecursive(service_mutex);
    return ESP_OK;
}

esp_err_t service_manager_set_auto_restart(const char *name, bool enable, int max_restarts, uint32_t delay_ms)
{
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    
    int index = find_service_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Service not found: %s", name);
        xSemaphoreGiveRecursive(service_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    service_info_t *service = &services[index];
    service->auto_restart = enable;
    service->max_restarts = max_restarts;
    service->restart_delay_ms = delay_ms;
    
    ESP_LOGI(TAG, "Auto-restart %s for %s (max=%d, delay=%lu ms)", 
             enable ? "enabled" : "disabled", name, max_restarts, delay_ms);
    
    xSemaphoreGiveRecursive(service_mutex);
    return ESP_OK;
}

esp_err_t service_manager_set_resource_limits(const char *name, uint32_t max_memory_bytes, uint32_t cpu_usage_percent)
{
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    
    int index = find_service_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Service not found: %s", name);
        xSemaphoreGiveRecursive(service_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    service_info_t *service = &services[index];
    service->max_memory_bytes = max_memory_bytes;
    service->cpu_usage_percent = cpu_usage_percent;
    service->resource_limited = (max_memory_bytes > 0 || cpu_usage_percent > 0);
    
    ESP_LOGI(TAG, "Resource limits set for %s: memory=%lu bytes, CPU=%lu%%", 
             name, max_memory_bytes, cpu_usage_percent);
    
    xSemaphoreGiveRecursive(service_mutex);
    return ESP_OK;
}

esp_err_t service_manager_set_health_check(const char *name, bool (*health_check)(void *arg), uint32_t interval_ms)
{
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    
    int index = find_service_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Service not found: %s", name);
        xSemaphoreGiveRecursive(service_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    service_info_t *service = &services[index];
    service->health_check = health_check;
    service->health_check_interval_ms = interval_ms;
    
    ESP_LOGI(TAG, "Health check set for %s: interval=%lu ms", name, interval_ms);
    
    xSemaphoreGiveRecursive(service_mutex);
    return ESP_OK;
}

void service_manager_pause_low_priority(int min_priority)
{
    ESP_LOGI(TAG, "Pausing services with priority below %d", min_priority);
    
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    
    for (int i = 0; i < service_count; i++) {
        if (services[i].state == SERVICE_STATE_RUNNING && services[i].priority < min_priority) {
            xSemaphoreGiveRecursive(service_mutex);
            service_manager_pause(services[i].name);
            xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
        }
    }
    
    xSemaphoreGiveRecursive(service_mutex);
}

void service_manager_resume_all(void)
{
    ESP_LOGI(TAG, "Resuming all paused services");
    
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    
    for (int i = 0; i < service_count; i++) {
        if (services[i].state == SERVICE_STATE_PAUSED) {
            xSemaphoreGiveRecursive(service_mutex);
            service_manager_resume(services[i].name);
            xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
        }
    }
    
    xSemaphoreGiveRecursive(service_mutex);
}

void service_manager_register_event_callback(service_event_cb_t cb, void *arg)
{
    event_cb = cb;
    event_cb_arg = arg;
    ESP_LOGI(TAG, "Service event callback registered");
}

void service_manager_run_health_checks(void)
{
    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
    
    uint64_t now = esp_timer_get_time();
    
    for (int i = 0; i < service_count; i++) {
        service_info_t *service = &services[i];
        
        if (service->state != SERVICE_STATE_RUNNING) continue;
        if (!service->health_check) continue;
        if (service->health_check_interval_ms == 0) continue;
        
        if ((now - service->last_health_check) >= (uint64_t)service->health_check_interval_ms * 1000ULL) {
            service->last_health_check = now;
            
            xSemaphoreGiveRecursive(service_mutex);
            bool health_ok = service->health_check(service->arg);
            xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
            
            if (!health_ok) {
                ESP_LOGE(TAG, "Health check failed for service: %s", service->name);
                
                if (service->auto_restart) {
                    xSemaphoreGiveRecursive(service_mutex);
                    service_crash_handler(service);
                    xSemaphoreTakeRecursive(service_mutex, portMAX_DELAY);
                } else {
                    service->state = SERVICE_STATE_CRASHED;
                    notify_event(service->name, SERVICE_STATE_CRASHED);
                }
            }
        }
    }
    
    xSemaphoreGiveRecursive(service_mutex);
}