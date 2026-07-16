#include "task_manager.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "TASK_MANAGER";

static task_info_t tasks[MAX_TASKS];
static int task_count = 0;
static int current_task_index = -1;
static SemaphoreHandle_t task_mutex = NULL;

#define LOW_MEMORY_THRESHOLD 1024 * 64
#define MAX_SUSPENDED_TASKS 3

esp_err_t task_manager_init(void)
{
    memset(tasks, 0, sizeof(tasks));
    task_count = 0;
    current_task_index = -1;
    
    task_mutex = xSemaphoreCreateRecursiveMutex();
    if (!task_mutex) {
        ESP_LOGE(TAG, "Failed to create task mutex");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Task manager initialized");
    return ESP_OK;
}

static int find_task_index(const char *name)
{
    for (int i = 0; i < task_count; i++) {
        if (strcmp(tasks[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

esp_err_t task_manager_add_task(const char *name, lv_obj_t *screen)
{
    if (!name || !screen) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTakeRecursive(task_mutex, portMAX_DELAY);

    if (task_count >= MAX_TASKS) {
        ESP_LOGE(TAG, "Max tasks reached");
        xSemaphoreGiveRecursive(task_mutex);
        return ESP_ERR_NO_MEM;
    }

    int existing_index = find_task_index(name);
    if (existing_index >= 0) {
        tasks[existing_index].screen = screen;
        tasks[existing_index].last_access_time = xTaskGetTickCount();
        tasks[existing_index].suspended = false;
        ESP_LOGI(TAG, "Updated existing task: %s", name);
        xSemaphoreGiveRecursive(task_mutex);
        return ESP_OK;
    }

    strncpy(tasks[task_count].name, name, TASK_NAME_MAX - 1);
    tasks[task_count].screen = screen;
    tasks[task_count].suspended = false;
    tasks[task_count].last_access_time = xTaskGetTickCount();
    tasks[task_count].memory_usage = 0;
    tasks[task_count].priority = task_count;

    task_count++;
    ESP_LOGI(TAG, "Added task: %s, total tasks: %d", name, task_count);

    xSemaphoreGiveRecursive(task_mutex);
    task_manager_check_memory();

    return ESP_OK;
}

esp_err_t task_manager_switch_task(const char *name)
{
    xSemaphoreTakeRecursive(task_mutex, portMAX_DELAY);

    int index = find_task_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Task not found: %s", name);
        xSemaphoreGiveRecursive(task_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    if (current_task_index >= 0 && current_task_index != index) {
        tasks[current_task_index].suspended = true;
        tasks[current_task_index].last_access_time = xTaskGetTickCount();
        ESP_LOGI(TAG, "Suspended task: %s", tasks[current_task_index].name);
    }

    current_task_index = index;
    tasks[index].suspended = false;
    tasks[index].last_access_time = xTaskGetTickCount();

    lv_obj_t *screen = tasks[index].screen;
    xSemaphoreGiveRecursive(task_mutex);
    
    lv_scr_load(screen);

    ESP_LOGI(TAG, "Switched to task: %s", name);
    return ESP_OK;
}

esp_err_t task_manager_remove_task(const char *name)
{
    xSemaphoreTakeRecursive(task_mutex, portMAX_DELAY);

    int index = find_task_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Task not found: %s", name);
        xSemaphoreGiveRecursive(task_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    lv_obj_t *screen = tasks[index].screen;
    xSemaphoreGiveRecursive(task_mutex);
    
    if (screen) {
        lv_obj_del(screen);
    }

    xSemaphoreTakeRecursive(task_mutex, portMAX_DELAY);
    
    for (int i = index; i < task_count - 1; i++) {
        tasks[i] = tasks[i + 1];
    }

    task_count--;

    if (current_task_index == index) {
        current_task_index = -1;
    } else if (current_task_index > index) {
        current_task_index--;
    }

    ESP_LOGI(TAG, "Removed task: %s, total tasks: %d", name, task_count);
    
    xSemaphoreGiveRecursive(task_mutex);
    return ESP_OK;
}

esp_err_t task_manager_suspend_task(const char *name)
{
    xSemaphoreTakeRecursive(task_mutex, portMAX_DELAY);

    int index = find_task_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Task not found: %s", name);
        xSemaphoreGiveRecursive(task_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    if (index == current_task_index) {
        ESP_LOGE(TAG, "Cannot suspend current task");
        xSemaphoreGiveRecursive(task_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    tasks[index].suspended = true;
    ESP_LOGI(TAG, "Suspended task: %s", name);
    
    xSemaphoreGiveRecursive(task_mutex);
    return ESP_OK;
}

esp_err_t task_manager_resume_task(const char *name)
{
    xSemaphoreTakeRecursive(task_mutex, portMAX_DELAY);

    int index = find_task_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Task not found: %s", name);
        xSemaphoreGiveRecursive(task_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    tasks[index].suspended = false;
    ESP_LOGI(TAG, "Resumed task: %s", name);
    
    xSemaphoreGiveRecursive(task_mutex);
    return ESP_OK;
}

int task_manager_get_task_count(void)
{
    xSemaphoreTakeRecursive(task_mutex, portMAX_DELAY);
    int count = task_count;
    xSemaphoreGiveRecursive(task_mutex);
    return count;
}

const task_info_t *task_manager_get_task(int index)
{
    xSemaphoreTakeRecursive(task_mutex, portMAX_DELAY);
    const task_info_t *result = NULL;
    if (index >= 0 && index < task_count) {
        result = &tasks[index];
    }
    xSemaphoreGiveRecursive(task_mutex);
    return result;
}

void task_manager_check_memory(void)
{
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    if (free_heap < LOW_MEMORY_THRESHOLD) {
        ESP_LOGW(TAG, "Low memory detected: %zu bytes, threshold: %u", 
                 free_heap, LOW_MEMORY_THRESHOLD);

        xSemaphoreTakeRecursive(task_mutex, portMAX_DELAY);

        int suspended_count = 0;
        for (int i = 0; i < task_count; i++) {
            if (tasks[i].suspended) {
                suspended_count++;
            }
        }

        if (suspended_count >= MAX_SUSPENDED_TASKS) {
            uint32_t oldest_time = xTaskGetTickCount();
            int oldest_index = -1;

            for (int i = 0; i < task_count; i++) {
                if (tasks[i].suspended && i != current_task_index && 
                    tasks[i].last_access_time < oldest_time) {
                    oldest_time = tasks[i].last_access_time;
                    oldest_index = i;
                }
            }

            if (oldest_index >= 0) {
                char task_name[TASK_NAME_MAX];
                strncpy(task_name, tasks[oldest_index].name, TASK_NAME_MAX - 1);
                task_name[TASK_NAME_MAX - 1] = '\0';
                
                xSemaphoreGiveRecursive(task_mutex);
                ESP_LOGW(TAG, "Auto-recycling task due to low memory: %s", task_name);
                task_manager_remove_task(task_name);
                return;
            }
        }

        xSemaphoreGiveRecursive(task_mutex);
    }
}