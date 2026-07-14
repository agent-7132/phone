#include "app_manager.h"
#include "home_screen.h"
#include "esp_log.h"

static const char *TAG = "APP_MANAGER";

static app_info_t apps[MAX_APPS];
static int app_count = 0;
static lv_obj_t *current_screen = NULL;
static lv_obj_t *previous_screen = NULL;
static const app_info_t *current_app = NULL;

void app_manager_init(void)
{
    app_count = 0;
    current_screen = NULL;
    previous_screen = NULL;
    current_app = NULL;
    ESP_LOGI(TAG, "App manager initialized");
}

void app_manager_register(const app_info_t *app)
{
    if (app_count < MAX_APPS) {
        apps[app_count++] = *app;
        ESP_LOGI(TAG, "Registered app: %s (%s)", app->name, 
                 app->type == APP_TYPE_NATIVE ? "native" : "dynamic");
    } else {
        ESP_LOGE(TAG, "Max apps reached, cannot register: %s", app->name);
    }
}

void app_manager_open_app(const char *name)
{
    for (int i = 0; i < app_count; i++) {
        if (strcmp(apps[i].name, name) == 0) {
            previous_screen = current_screen;
            current_app = &apps[i];
            
            if (current_screen) {
                lv_obj_del(current_screen);
            }
            
            if (current_app->type == APP_TYPE_NATIVE) {
                current_screen = current_app->data.native.create_screen();
                if (current_app->data.native.on_enter) {
                    current_app->data.native.on_enter();
                }
            } else {
                extern lv_obj_t *dynamic_app_create_screen(const app_info_t *app);
                current_screen = dynamic_app_create_screen(current_app);
            }
            
            lv_scr_load(current_screen);
            ESP_LOGI(TAG, "Opened app: %s", name);
            return;
        }
    }
    ESP_LOGE(TAG, "App not found: %s", name);
}

void app_manager_go_home(void)
{
    if (current_app && current_app->type == APP_TYPE_NATIVE && 
        current_app->data.native.on_exit) {
        current_app->data.native.on_exit();
    }
    
    if (current_screen) {
        lv_obj_del(current_screen);
    }
    
    current_screen = home_screen_create();
    lv_scr_load(current_screen);
    current_app = NULL;
    previous_screen = NULL;
    
    ESP_LOGI(TAG, "Returned to home screen");
}

void app_manager_go_back(void)
{
    if (previous_screen) {
        if (current_app && current_app->type == APP_TYPE_NATIVE && 
            current_app->data.native.on_exit) {
            current_app->data.native.on_exit();
        }
        
        if (current_screen) {
            lv_obj_del(current_screen);
        }
        
        current_screen = previous_screen;
        lv_scr_load(current_screen);
        previous_screen = NULL;
        current_app = NULL;
        
        ESP_LOGI(TAG, "Went back to previous screen");
    } else {
        app_manager_go_home();
    }
}

int app_manager_get_app_count(void)
{
    return app_count;
}

const app_info_t *app_manager_get_app(int index)
{
    if (index >= 0 && index < app_count) {
        return &apps[index];
    }
    return NULL;
}

const app_info_t *app_manager_find_app(const char *name)
{
    for (int i = 0; i < app_count; i++) {
        if (strcmp(apps[i].name, name) == 0) {
            return &apps[i];
        }
    }
    return NULL;
}
