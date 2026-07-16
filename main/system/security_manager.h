#pragma once

#include "esp_err.h"
#include "permission_manager.h"
#include "memory_protection.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SECURITY_LEVEL_NONE,
    SECURITY_LEVEL_LOW,
    SECURITY_LEVEL_MEDIUM,
    SECURITY_LEVEL_HIGH
} security_level_t;

typedef struct {
    char app_name[32];
    security_level_t level;
    uint32_t permissions;
    bool isolated;
    bool memory_protected;
} app_security_info_t;

esp_err_t security_manager_init(void);

esp_err_t security_manager_register_app(const char *app_name, 
                                        security_level_t level,
                                        uint32_t required_permissions);

esp_err_t security_manager_unregister_app(const char *app_name);

bool security_manager_check_permission(const char *app_name, permission_t perm);

esp_err_t security_manager_grant_permission(const char *app_name, permission_t perm);

esp_err_t security_manager_revoke_permission(const char *app_name, permission_t perm);

esp_err_t security_manager_add_memory_region(void *start_addr, 
                                             size_t size, 
                                             mem_region_type_t type);

esp_err_t security_manager_remove_memory_region(void *start_addr);

void security_manager_enable_leak_detection(bool enable);

void security_manager_dump_security_info(void);

#ifdef __cplusplus
}
#endif