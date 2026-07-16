#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "app_manager.h"
#include "home_screen.h"
#include "task_manager.h"
#include "esp_log.h"

static const char *TAG = "APP_MANAGER";

static app_info_t apps[MAX_APPS];
static int app_count = 0;
static lv_obj_t *current_screen = NULL;
static lv_obj_t *previous_screen = NULL;
static const app_info_t *current_app = NULL;
static SemaphoreHandle_t app_mutex = NULL;

void app_manager_init(void)
{
    app_count = 0;
    current_screen = NULL;
    previous_screen = NULL;
    current_app = NULL;
    
    app_mutex = xSemaphoreCreateRecursiveMutex();
    if (!app_mutex) {
        ESP_LOGE(TAG, "Failed to create app mutex");
        return;
    }
    
    task_manager_init();
    ESP_LOGI(TAG, "App manager initialized");
}

void app_manager_register(const app_info_t *app)
{
    xSemaphoreTakeRecursive(app_mutex, portMAX_DELAY);
    
    if (app_count < MAX_APPS) {
        apps[app_count++] = *app;
        ESP_LOGI(TAG, "Registered app: %s (%s)", app->name, 
                 app->type == APP_TYPE_NATIVE ? "native" : "dynamic");
    } else {
        ESP_LOGE(TAG, "Max apps reached, cannot register: %s", app->name);
    }
    
    xSemaphoreGiveRecursive(app_mutex);
}

void app_manager_open_app(const char *name)
{
    xSemaphoreTakeRecursive(app_mutex, portMAX_DELAY);
    
    for (int i = 0; i < app_count; i++) {
        if (strcmp(apps[i].name, name) == 0) {
            const app_info_t *new_app = &apps[i];
            
            const task_info_t *existing_task = NULL;
            for (int j = 0; j < task_manager_get_task_count(); j++) {
                const task_info_t *task = task_manager_get_task(j);
                if (task && strcmp(task->name, name) == 0) {
                    existing_task = task;
                    break;
                }
            }
            
            if (existing_task && !existing_task->suspended && existing_task->screen) {
                if (current_screen && current_screen != existing_task->screen) {
                    lv_obj_del(current_screen);
                }
                previous_screen = NULL;
                current_screen = existing_task->screen;
                current_app = new_app;
                
                xSemaphoreGiveRecursive(app_mutex);
                task_manager_switch_task(name);
                ESP_LOGI(TAG, "Switched to existing task: %s", name);
                return;
            }
            
            if (current_screen && current_app) {
                task_manager_add_task(current_app->name, current_screen);
            }
            
            previous_screen = current_screen;
            current_screen = NULL;
            current_app = new_app;
            
            xSemaphoreGiveRecursive(app_mutex);
            
            if (current_app->type == APP_TYPE_NATIVE) {
                current_screen = current_app->data.native.create_screen();
                if (current_app->data.native.on_enter) {
                    current_app->data.native.on_enter();
                }
            } else {
                extern lv_obj_t *dynamic_app_create_screen(const app_info_t *app);
                current_screen = dynamic_app_create_screen(current_app);
            }
            
            xSemaphoreTakeRecursive(app_mutex, portMAX_DELAY);
            task_manager_add_task(name, current_screen);
            lv_scr_load(current_screen);
            ESP_LOGI(TAG, "Opened app: %s", name);
            
            xSemaphoreGiveRecursive(app_mutex);
            return;
        }
    }
    
    ESP_LOGE(TAG, "App not found: %s", name);
    xSemaphoreGiveRecursive(app_mutex);
}

void app_manager_go_home(void)
{
    xSemaphoreTakeRecursive(app_mutex, portMAX_DELAY);
    
    if (current_app) {
        if (current_app->type == APP_TYPE_NATIVE && 
            current_app->data.native.on_exit) {
            current_app->data.native.on_exit();
        } else if (current_app->type == APP_TYPE_DYNAMIC && 
                   current_app->data.dynamic.on_exit) {
            current_app->data.dynamic.on_exit();
        }
    }
    
    if (current_screen && current_app) {
        task_manager_add_task(current_app->name, current_screen);
    } else if (current_screen) {
        lv_obj_del(current_screen);
    }
    
    xSemaphoreGiveRecursive(app_mutex);
    
    current_screen = home_screen_create();
    lv_scr_load(current_screen);
    
    xSemaphoreTakeRecursive(app_mutex, portMAX_DELAY);
    current_app = NULL;
    previous_screen = NULL;
    
    ESP_LOGI(TAG, "Returned to home screen");
    xSemaphoreGiveRecursive(app_mutex);
}

void app_manager_go_back(void)
{
    xSemaphoreTakeRecursive(app_mutex, portMAX_DELAY);
    
    if (previous_screen) {
        if (current_app) {
            if (current_app->type == APP_TYPE_NATIVE && 
                current_app->data.native.on_exit) {
                current_app->data.native.on_exit();
            } else if (current_app->type == APP_TYPE_DYNAMIC && 
                       current_app->data.dynamic.on_exit) {
                current_app->data.dynamic.on_exit();
            }
        }
        
        if (current_screen && current_app) {
            task_manager_add_task(current_app->name, current_screen);
        } else if (current_screen) {
            lv_obj_del(current_screen);
        }
        
        current_screen = previous_screen;
        lv_scr_load(current_screen);
        previous_screen = NULL;
        current_app = NULL;
        
        ESP_LOGI(TAG, "Went back to previous screen");
        xSemaphoreGiveRecursive(app_mutex);
    } else {
        xSemaphoreGiveRecursive(app_mutex);
        app_manager_go_home();
    }
}

int app_manager_get_app_count(void)
{
    xSemaphoreTakeRecursive(app_mutex, portMAX_DELAY);
    int count = app_count;
    xSemaphoreGiveRecursive(app_mutex);
    return count;
}

const app_info_t *app_manager_get_app(int index)
{
    xSemaphoreTakeRecursive(app_mutex, portMAX_DELAY);
    const app_info_t *result = NULL;
    if (index >= 0 && index < app_count) {
        result = &apps[index];
    }
    xSemaphoreGiveRecursive(app_mutex);
    return result;
}

const app_info_t *app_manager_find_app(const char *name)
{
    xSemaphoreTakeRecursive(app_mutex, portMAX_DELAY);
    
    const app_info_t *result = NULL;
    for (int i = 0; i < app_count; i++) {
        if (strcmp(apps[i].name, name) == 0) {
            result = &apps[i];
            break;
        }
    }
    
    xSemaphoreGiveRecursive(app_mutex);
    return result;
}