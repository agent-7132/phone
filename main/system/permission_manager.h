#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PERMISSION_MANAGER_MAX_APPS 20
#define PERMISSION_NVS_NAMESPACE "perm_mgr"

typedef enum {
    PERMISSION_NETWORK = 0x0001,
    PERMISSION_BLUETOOTH = 0x0002,
    PERMISSION_CAMERA = 0x0004,
    PERMISSION_STORAGE = 0x0008,
    PERMISSION_SENSORS = 0x0010,
    PERMISSION_AUDIO = 0x0020,
    PERMISSION_LOCATION = 0x0040,
    PERMISSION_NOTIFICATION = 0x0080
} permission_t;

typedef enum {
    PERMISSION_STATUS_DENIED,
    PERMISSION_STATUS_GRANTED,
    PERMISSION_STATUS_PENDING
} permission_status_t;

typedef enum {
    PERMISSION_CHANGE_GRANTED,
    PERMISSION_CHANGE_REVOKED,
    PERMISSION_CHANGE_REQUESTED
} permission_change_type_t;

typedef struct {
    char app_name[32];
    uint32_t permissions;
    uint32_t granted_permissions;
} permission_info_t;

typedef struct {
    permission_t permission;
    const char *name;
    const char *description;
} permission_desc_t;

typedef void (*permission_change_cb_t)(const char *app_name, 
                                       permission_t permission, 
                                       permission_change_type_t change_type,
                                       void *arg);

esp_err_t permission_manager_init(void);

bool permission_check(const char *app_name, permission_t perm);

esp_err_t permission_request(const char *app_name, permission_t perm);

esp_err_t permission_grant(const char *app_name, permission_t perm);

esp_err_t permission_revoke(const char *app_name, permission_t perm);

esp_err_t permission_register_app(const char *app_name, uint32_t required_permissions);

esp_err_t permission_unregister_app(const char *app_name);

permission_status_t permission_get_status(const char *app_name, permission_t perm);

esp_err_t permission_get_app_info(const char *app_name, permission_info_t *info);

int permission_get_app_count(void);

esp_err_t permission_get_app_info_by_index(int index, permission_info_t *info);

void permission_dump_permissions(void);

const permission_desc_t *permission_get_desc(permission_t perm);

const char *permission_get_name(permission_t perm);

const char *permission_get_description(permission_t perm);

void permission_register_change_callback(permission_change_cb_t cb, void *arg);

esp_err_t permission_save_to_nvs(void);

esp_err_t permission_load_from_nvs(void);

esp_err_t permission_set_default_permissions(const char *app_name, uint32_t permissions);

#ifdef __cplusplus
}
#endif