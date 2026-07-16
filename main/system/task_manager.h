#pragma once

#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TASKS 10
#define TASK_NAME_MAX 32

typedef struct {
    char name[TASK_NAME_MAX];
    lv_obj_t *screen;
    bool suspended;
    uint32_t last_access_time;
    uint32_t memory_usage;
    int priority;
} task_info_t;

esp_err_t task_manager_init(void);
esp_err_t task_manager_add_task(const char *name, lv_obj_t *screen);
esp_err_t task_manager_switch_task(const char *name);
esp_err_t task_manager_remove_task(const char *name);
esp_err_t task_manager_suspend_task(const char *name);
esp_err_t task_manager_resume_task(const char *name);
int task_manager_get_task_count(void);
const task_info_t *task_manager_get_task(int index);
void task_manager_check_memory(void);

#ifdef __cplusplus
}
#endif