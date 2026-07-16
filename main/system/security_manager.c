#include "security_manager.h"
#include "esp_log.h"

static const char *TAG = "SECURITY_MANAGER";

#define MAX_SECURITY_APPS 20

static app_security_info_t security_apps[MAX_SECURITY_APPS];
static int security_app_count = 0;

static int find_app_index(const char *app_name)
{
    for (int i = 0; i < security_app_count; i++) {
        if (strcmp(security_apps[i].app_name, app_name) == 0) {
            return i;
        }
    }
    return -1;
}

esp_err_t security_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing security manager...");

    security_app_count = 0;
    memset(security_apps, 0, sizeof(security_apps));

    esp_err_t ret = permission_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize permission manager");
        return ret;
    }

    ret = memory_protection_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize memory protection");
        return ret;
    }

    ESP_LOGI(TAG, "Security manager initialized");
    return ESP_OK;
}

esp_err_t security_manager_register_app(const char *app_name, 
                                        security_level_t level,
                                        uint32_t required_permissions)
{
    if (!app_name) {
        ESP_LOGE(TAG, "App name is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (security_app_count >= MAX_SECURITY_APPS) {
        ESP_LOGE(TAG, "Maximum security apps reached");
        return ESP_ERR_NO_MEM;
    }

    int existing_index = find_app_index(app_name);
    if (existing_index >= 0) {
        security_apps[existing_index].level = level;
        security_apps[existing_index].permissions = required_permissions;
        ESP_LOGI(TAG, "Updated security info for app: %s (level=%d)", app_name, level);
        return ESP_OK;
    }

    app_security_info_t *info = &security_apps[security_app_count];
    strncpy(info->app_name, app_name, sizeof(info->app_name) - 1);
    info->app_name[sizeof(info->app_name) - 1] = '\0';
    info->level = level;
    info->permissions = required_permissions;
    info->isolated = (level >= SECURITY_LEVEL_MEDIUM);
    info->memory_protected = (level >= SECURITY_LEVEL_HIGH);

    security_app_count++;

    esp_err_t ret = permission_register_app(app_name, required_permissions);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register app permissions");
        security_app_count--;
        return ret;
    }

    ESP_LOGI(TAG, "Registered app: %s (level=%d, permissions=0x%08x, isolated=%d, memory_protected=%d)", 
             app_name, level, required_permissions, info->isolated, info->memory_protected);

    return ESP_OK;
}

esp_err_t security_manager_unregister_app(const char *app_name)
{
    int index = find_app_index(app_name);
    if (index < 0) {
        ESP_LOGE(TAG, "App not found: %s", app_name);
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t ret = permission_unregister_app(app_name);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unregister app permissions");
        return ret;
    }

    for (int i = index; i < security_app_count - 1; i++) {
        security_apps[i] = security_apps[i + 1];
    }
    security_app_count--;

    ESP_LOGI(TAG, "Unregistered app: %s", app_name);
    return ESP_OK;
}

bool security_manager_check_permission(const char *app_name, permission_t perm)
{
    return permission_check(app_name, perm);
}

esp_err_t security_manager_grant_permission(const char *app_name, permission_t perm)
{
    return permission_grant(app_name, perm);
}

esp_err_t security_manager_revoke_permission(const char *app_name, permission_t perm)
{
    return permission_revoke(app_name, perm);
}

esp_err_t security_manager_add_memory_region(void *start_addr, 
                                             size_t size, 
                                             mem_region_type_t type)
{
    return memory_protection_add_region(start_addr, size, type);
}

esp_err_t security_manager_remove_memory_region(void *start_addr)
{
    return memory_protection_remove_region(start_addr);
}

void security_manager_enable_leak_detection(bool enable)
{
    memory_protection_enable_leak_detection(enable);
}

void security_manager_dump_security_info(void)
{
    ESP_LOGI(TAG, "=== Security Manager Dump ===");
    ESP_LOGI(TAG, "Total security apps: %d", security_app_count);

    for (int i = 0; i < security_app_count; i++) {
        const char *level_str = "";
        switch (security_apps[i].level) {
            case SECURITY_LEVEL_NONE: level_str = "NONE"; break;
            case SECURITY_LEVEL_LOW: level_str = "LOW"; break;
            case SECURITY_LEVEL_MEDIUM: level_str = "MEDIUM"; break;
            case SECURITY_LEVEL_HIGH: level_str = "HIGH"; break;
        }

        ESP_LOGI(TAG, "[%d] %s - Level: %s", i, security_apps[i].app_name, level_str);
        ESP_LOGI(TAG, "  Permissions: 0x%08x", security_apps[i].permissions);
        ESP_LOGI(TAG, "  Memory Protected: %d", security_apps[i].memory_protected);
    }

    permission_dump_permissions();
    memory_protection_dump_allocations();

    ESP_LOGI(TAG, "=== End Security Dump ===");
}