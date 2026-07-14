#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_APPS 20
#define APP_NAME_MAX 32
#define APP_TITLE_MAX 64
#define APP_VERSION_MAX 16
#define APP_AUTHOR_MAX 32
#define APP_DESC_MAX 128

typedef enum {
    APP_TYPE_NATIVE,
    APP_TYPE_DYNAMIC
} app_type_t;

typedef struct {
    app_type_t type;
    char name[APP_NAME_MAX];
    char title[APP_TITLE_MAX];
    char version[APP_VERSION_MAX];
    char author[APP_AUTHOR_MAX];
    char description[APP_DESC_MAX];
    char icon[16];
    char path[128];
    
    union {
        struct {
            lv_obj_t *(*create_screen)(void);
            void (*on_enter)(void);
            void (*on_exit)(void);
        } native;
        struct {
            char entry_point[128];
            void *engine_data;
        } dynamic;
    } data;
} app_info_t;

void app_manager_init(void);
void app_manager_register(const app_info_t *app);
void app_manager_open_app(const char *name);
void app_manager_go_home(void);
void app_manager_go_back(void);

int app_manager_get_app_count(void);
const app_info_t *app_manager_get_app(int index);
const app_info_t *app_manager_find_app(const char *name);

#ifdef __cplusplus
}
#endif
