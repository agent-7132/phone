#include "permission_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "PERMISSION_MANAGER";

static permission_info_t apps[PERMISSION_MANAGER_MAX_APPS];
static int app_count = 0;
static permission_change_cb_t change_cb = NULL;
static void *change_cb_arg = NULL;

static const permission_desc_t permission_descs[] = {
    {PERMISSION_NETWORK, "Network", "Access to network services"},
    {PERMISSION_BLUETOOTH, "Bluetooth", "Access to Bluetooth services"},
    {PERMISSION_CAMERA, "Camera", "Access to camera hardware"},
    {PERMISSION_STORAGE, "Storage", "Access to storage (SD card/SPIFFS)"},
    {PERMISSION_SENSORS, "Sensors", "Access to sensor data"},
    {PERMISSION_AUDIO, "Audio", "Access to audio recording/playback"},
    {PERMISSION_LOCATION, "Location", "Access to location services"},
    {PERMISSION_NOTIFICATION, "Notification", "Send system notifications"},
    {0, NULL, NULL}
};

static int find_app_index(const char *app_name)
{
    for (int i = 0; i < app_count; i++) {
        if (strcmp(apps[i].app_name, app_name) == 0) {
            return i;
        }
    }
    return -1;
}

static void notify_change(const char *app_name, permission_t perm, permission_change_type_t type)
{
    if (change_cb) {
        change_cb(app_name, perm, type, change_cb_arg);
    }
}

esp_err_t permission_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing permission manager...");
    
    app_count = 0;
    memset(apps, 0, sizeof(apps));
    change_cb = NULL;
    change_cb_arg = NULL;
    
    esp_err_t ret = permission_load_from_nvs();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load permissions from NVS: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "Permission manager initialized");
    return ESP_OK;
}

bool permission_check(const char *app_name, permission_t perm)
{
    int index = find_app_index(app_name);
    if (index < 0) {
        ESP_LOGW(TAG, "App not registered: %s", app_name);
        return false;
    }
    
    if (apps[index].granted_permissions & perm) {
        return true;
    }
    
    ESP_LOGW(TAG, "Permission denied for app %s: %s", app_name, permission_get_name(perm));
    return false;
}

esp_err_t permission_request(const char *app_name, permission_t perm)
{
    int index = find_app_index(app_name);
    if (index < 0) {
        ESP_LOGE(TAG, "App not registered: %s", app_name);
        return ESP_ERR_NOT_FOUND;
    }
    
    if (!(apps[index].permissions & perm)) {
        ESP_LOGE(TAG, "Permission %s not required by app: %s", permission_get_name(perm), app_name);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (apps[index].granted_permissions & perm) {
        ESP_LOGI(TAG, "Permission %s already granted: %s", permission_get_name(perm), app_name);
        return ESP_OK;
    }
    
    apps[index].granted_permissions |= perm;
    
    ESP_LOGI(TAG, "Permission %s granted: %s", permission_get_name(perm), app_name);
    
    notify_change(app_name, perm, PERMISSION_CHANGE_GRANTED);
    permission_save_to_nvs();
    
    return ESP_OK;
}

esp_err_t permission_grant(const char *app_name, permission_t perm)
{
    int index = find_app_index(app_name);
    if (index < 0) {
        ESP_LOGE(TAG, "App not registered: %s", app_name);
        return ESP_ERR_NOT_FOUND;
    }
    
    bool was_granted = (apps[index].granted_permissions & perm) != 0;
    apps[index].granted_permissions |= perm;
    
    ESP_LOGI(TAG, "Permission %s explicitly granted: %s", permission_get_name(perm), app_name);
    
    if (!was_granted) {
        notify_change(app_name, perm, PERMISSION_CHANGE_GRANTED);
        permission_save_to_nvs();
    }
    
    return ESP_OK;
}

esp_err_t permission_revoke(const char *app_name, permission_t perm)
{
    int index = find_app_index(app_name);
    if (index < 0) {
        ESP_LOGE(TAG, "App not registered: %s", app_name);
        return ESP_ERR_NOT_FOUND;
    }
    
    bool was_granted = (apps[index].granted_permissions & perm) != 0;
    apps[index].granted_permissions &= ~perm;
    
    ESP_LOGI(TAG, "Permission %s revoked: %s", permission_get_name(perm), app_name);
    
    if (was_granted) {
        notify_change(app_name, perm, PERMISSION_CHANGE_REVOKED);
        permission_save_to_nvs();
    }
    
    return ESP_OK;
}

esp_err_t permission_register_app(const char *app_name, uint32_t required_permissions)
{
    if (!app_name) {
        ESP_LOGE(TAG, "App name is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (app_count >= PERMISSION_MANAGER_MAX_APPS) {
        ESP_LOGE(TAG, "Maximum apps reached");
        return ESP_ERR_NO_MEM;
    }
    
    int existing_index = find_app_index(app_name);
    if (existing_index >= 0) {
        ESP_LOGW(TAG, "App already registered: %s", app_name);
        return ESP_OK;
    }
    
    permission_info_t *info = &apps[app_count];
    strncpy(info->app_name, app_name, sizeof(info->app_name) - 1);
    info->app_name[sizeof(info->app_name) - 1] = '\0';
    info->permissions = required_permissions;
    info->granted_permissions = 0;
    
    app_count++;
    
    ESP_LOGI(TAG, "Registered app: %s (permissions=0x%08x)", app_name, required_permissions);
    
    permission_save_to_nvs();
    
    return ESP_OK;
}

esp_err_t permission_unregister_app(const char *app_name)
{
    int index = find_app_index(app_name);
    if (index < 0) {
        ESP_LOGE(TAG, "App not found: %s", app_name);
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Unregistered app: %s", app_name);
    
    for (int i = index; i < app_count - 1; i++) {
        apps[i] = apps[i + 1];
    }
    app_count--;
    
    permission_save_to_nvs();
    
    return ESP_OK;
}

permission_status_t permission_get_status(const char *app_name, permission_t perm)
{
    int index = find_app_index(app_name);
    if (index < 0) {
        return PERMISSION_STATUS_DENIED;
    }
    
    if (apps[index].granted_permissions & perm) {
        return PERMISSION_STATUS_GRANTED;
    }
    
    return PERMISSION_STATUS_DENIED;
}

esp_err_t permission_get_app_info(const char *app_name, permission_info_t *info)
{
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int index = find_app_index(app_name);
    if (index < 0) {
        return ESP_ERR_NOT_FOUND;
    }
    
    *info = apps[index];
    
    return ESP_OK;
}

int permission_get_app_count(void)
{
    return app_count;
}

esp_err_t permission_get_app_info_by_index(int index, permission_info_t *info)
{
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (index < 0 || index >= app_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *info = apps[index];
    
    return ESP_OK;
}

void permission_dump_permissions(void)
{
    ESP_LOGI(TAG, "=== Permission Manager Dump ===");
    ESP_LOGI(TAG, "Total apps: %d", app_count);
    
    for (int i = 0; i < app_count; i++) {
        ESP_LOGI(TAG, "[%d] %s", i, apps[i].app_name);
        ESP_LOGI(TAG, "  Required: 0x%08x", apps[i].permissions);
        ESP_LOGI(TAG, "  Granted:  0x%08x", apps[i].granted_permissions);
        
        for (int j = 0; permission_descs[j].name; j++) {
            if (apps[i].granted_permissions & permission_descs[j].permission) {
                ESP_LOGI(TAG, "    ✓ %s", permission_descs[j].name);
            } else if (apps[i].permissions & permission_descs[j].permission) {
                ESP_LOGI(TAG, "    ✗ %s", permission_descs[j].name);
            }
        }
    }
    
    ESP_LOGI(TAG, "=== End Dump ===");
}

const permission_desc_t *permission_get_desc(permission_t perm)
{
    for (int i = 0; permission_descs[i].name; i++) {
        if (permission_descs[i].permission == perm) {
            return &permission_descs[i];
        }
    }
    return NULL;
}

const char *permission_get_name(permission_t perm)
{
    const permission_desc_t *desc = permission_get_desc(perm);
    return desc ? desc->name : "UNKNOWN";
}

const char *permission_get_description(permission_t perm)
{
    const permission_desc_t *desc = permission_get_desc(perm);
    return desc ? desc->description : "No description";
}

void permission_register_change_callback(permission_change_cb_t cb, void *arg)
{
    change_cb = cb;
    change_cb_arg = arg;
    ESP_LOGI(TAG, "Permission change callback registered");
}

esp_err_t permission_save_to_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(PERMISSION_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_set_i32(handle, "app_count", app_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save app count: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }
    
    for (int i = 0; i < app_count; i++) {
        char key[64];
        snprintf(key, sizeof(key), "app_%d_name", i);
        ret = nvs_set_str(handle, key, apps[i].app_name);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save app %d name: %s", i, esp_err_to_name(ret));
            nvs_close(handle);
            return ret;
        }
        
        snprintf(key, sizeof(key), "app_%d_perms", i);
        ret = nvs_set_u32(handle, key, apps[i].permissions);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save app %d permissions: %s", i, esp_err_to_name(ret));
            nvs_close(handle);
            return ret;
        }
        
        snprintf(key, sizeof(key), "app_%d_granted", i);
        ret = nvs_set_u32(handle, key, apps[i].granted_permissions);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save app %d granted permissions: %s", i, esp_err_to_name(ret));
            nvs_close(handle);
            return ret;
        }
    }
    
    ret = nvs_commit(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }
    
    nvs_close(handle);
    ESP_LOGI(TAG, "Permissions saved to NVS");
    return ESP_OK;
}

esp_err_t permission_load_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(PERMISSION_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS (first run?): %s", esp_err_to_name(ret));
        return ret;
    }
    
    int32_t count = 0;
    ret = nvs_get_i32(handle, "app_count", &count);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to load app count: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }
    
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return ESP_OK;
    }
    
    app_count = 0;
    for (int i = 0; i < count && i < PERMISSION_MANAGER_MAX_APPS; i++) {
        char key[64];
        char name[32];
        uint32_t perms = 0;
        uint32_t granted = 0;
        
        snprintf(key, sizeof(key), "app_%d_name", i);
        size_t len = sizeof(name);
        ret = nvs_get_str(handle, key, name, &len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load app %d name: %s", i, esp_err_to_name(ret));
            nvs_close(handle);
            return ret;
        }
        
        snprintf(key, sizeof(key), "app_%d_perms", i);
        ret = nvs_get_u32(handle, key, &perms);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load app %d permissions: %s", i, esp_err_to_name(ret));
            nvs_close(handle);
            return ret;
        }
        
        snprintf(key, sizeof(key), "app_%d_granted", i);
        ret = nvs_get_u32(handle, key, &granted);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load app %d granted permissions: %s", i, esp_err_to_name(ret));
            nvs_close(handle);
            return ret;
        }
        
        strncpy(apps[app_count].app_name, name, sizeof(apps[app_count].app_name) - 1);
        apps[app_count].permissions = perms;
        apps[app_count].granted_permissions = granted;
        app_count++;
    }
    
    nvs_close(handle);
    ESP_LOGI(TAG, "Permissions loaded from NVS: %d apps", app_count);
    return ESP_OK;
}

esp_err_t permission_set_default_permissions(const char *app_name, uint32_t permissions)
{
    int index = find_app_index(app_name);
    if (index < 0) {
        ESP_LOGE(TAG, "App not registered: %s", app_name);
        return ESP_ERR_NOT_FOUND;
    }
    
    apps[index].granted_permissions = permissions;
    
    ESP_LOGI(TAG, "Default permissions set for %s: 0x%08x", app_name, permissions);
    
    permission_save_to_nvs();
    
    return ESP_OK;
}