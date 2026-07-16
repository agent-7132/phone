#include "app_installer.h"
#include "app_manager.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "dirent.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include "esp_heap_caps.h"

static const char *TAG = "APP_INSTALLER";

#define EPP_MAGIC "EPP1"
#define EPP_MAGIC_LEN 4

static void parse_string(const char **json, char *dest, int max_len)
{
    int i = 0;
    (*json)++;
    while (**json != '\"' && **json != '\0' && i < max_len - 1) {
        dest[i++] = **json;
        (*json)++;
    }
    dest[i] = '\0';
    if (**json == '\"') (*json)++;
}

static void skip_whitespace(const char **json)
{
    while (**json == ' ' || **json == '\t' || **json == '\n' || **json == '\r') {
        (*json)++;
    }
}

static void parse_key(const char **json, char *key, int max_len)
{
    skip_whitespace(json);
    if (**json == '\"') {
        parse_string(json, key, max_len);
    }
    skip_whitespace(json);
    if (**json == ':') (*json)++;
    skip_whitespace(json);
}

static install_result_t parse_manifest(const char *json, app_info_t *app)
{
    if (json == NULL) return INSTALL_ERR_INVALID_MANIFEST;
    
    memset(app, 0, sizeof(app_info_t));
    app->type = APP_TYPE_DYNAMIC;
    
    const char *p = json;
    skip_whitespace(&p);
    
    if (*p != '{') return INSTALL_ERR_INVALID_MANIFEST;
    p++;
    
    while (*p != '}' && *p != '\0') {
        skip_whitespace(&p);
        if (*p == ',') { p++; continue; }
        
        char key[32] = {0};
        parse_key(&p, key, sizeof(key));
        
        if (strcmp(key, "name") == 0) {
            parse_string(&p, app->name, APP_NAME_MAX);
        } else if (strcmp(key, "title") == 0) {
            parse_string(&p, app->title, APP_TITLE_MAX);
        } else if (strcmp(key, "version") == 0) {
            parse_string(&p, app->version, APP_VERSION_MAX);
        } else if (strcmp(key, "author") == 0) {
            parse_string(&p, app->author, APP_AUTHOR_MAX);
        } else if (strcmp(key, "description") == 0) {
            parse_string(&p, app->description, APP_DESC_MAX);
        } else if (strcmp(key, "icon") == 0) {
            parse_string(&p, app->icon, sizeof(app->icon));
        } else if (strcmp(key, "entry_point") == 0) {
            parse_string(&p, app->data.dynamic.entry_point, sizeof(app->data.dynamic.entry_point));
        } else {
            while (*p != ',' && *p != '}' && *p != '\0') p++;
        }
    }
    
    if (app->name[0] == '\0') return INSTALL_ERR_INVALID_MANIFEST;
    if (app->title[0] == '\0') strcpy(app->title, app->name);
    if (app->icon[0] == '\0') strcpy(app->icon, "📱");
    
    ESP_LOGI(TAG, "Parsed manifest: name=%s, title=%s, version=%s", 
             app->name, app->title, app->version);
    
    return INSTALL_OK;
}

static install_result_t read_manifest(const char *app_path, app_info_t *app)
{
    char manifest_path[256];
    snprintf(manifest_path, sizeof(manifest_path), "%s/%s", app_path, APP_MANIFEST_FILE);
    
    FILE *f = fopen(manifest_path, "r");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open manifest: %s", manifest_path);
        return INSTALL_ERR_NOT_FOUND;
    }
    
    char buffer[1024] = {0};
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    
    if (len == 0) {
        ESP_LOGE(TAG, "Empty manifest");
        return INSTALL_ERR_INVALID_MANIFEST;
    }
    
    return parse_manifest(buffer, app);
}

static int epp_extract(const char *epp_file, const char *dest_dir)
{
    FILE *f = fopen(epp_file, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open EPP file: %s", epp_file);
        return -1;
    }
    
    char magic[EPP_MAGIC_LEN + 1] = {0};
    if (fread(magic, 1, EPP_MAGIC_LEN, f) != EPP_MAGIC_LEN || 
        strcmp(magic, EPP_MAGIC) != 0) {
        ESP_LOGE(TAG, "Invalid EPP magic: %s", magic);
        fclose(f);
        return -1;
    }
    
    uint32_t file_count;
    if (fread(&file_count, sizeof(file_count), 1, f) != 1) {
        ESP_LOGE(TAG, "Cannot read file count");
        fclose(f);
        return -1;
    }
    
    for (uint32_t i = 0; i < file_count; i++) {
        uint16_t path_len;
        if (fread(&path_len, sizeof(path_len), 1, f) != 1) {
            ESP_LOGE(TAG, "Cannot read path length");
            fclose(f);
            return -1;
        }
        
        char *path = (char *)heap_caps_malloc(path_len + 1, MALLOC_CAP_SPIRAM);
        if (!path) {
            ESP_LOGE(TAG, "Cannot allocate memory for path");
            fclose(f);
            return -1;
        }
        
        if (fread(path, 1, path_len, f) != path_len) {
            ESP_LOGE(TAG, "Cannot read path");
            heap_caps_free(path);
            fclose(f);
            return -1;
        }
        path[path_len] = '\0';
        
        uint32_t content_len;
        if (fread(&content_len, sizeof(content_len), 1, f) != 1) {
            ESP_LOGE(TAG, "Cannot read content length");
            heap_caps_free(path);
            fclose(f);
            return -1;
        }
        
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dest_dir, path);
        full_path[sizeof(full_path) - 1] = '\0';
        
        size_t full_path_len = strlen(full_path) + 1;
        char *dir_path = heap_caps_malloc(full_path_len, MALLOC_CAP_SPIRAM);
        strcpy(dir_path, full_path);
        char *last_slash = strrchr(dir_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            mkdir(dir_path, 0755);
        }
        heap_caps_free(dir_path);
        
        char *content = (char *)heap_caps_malloc(content_len, MALLOC_CAP_SPIRAM);
        if (!content) {
            ESP_LOGE(TAG, "Cannot allocate memory for content in PSRAM");
            heap_caps_free(path);
            fclose(f);
            return -1;
        }
        
        if (fread(content, 1, content_len, f) != content_len) {
            ESP_LOGE(TAG, "Cannot read content");
            heap_caps_free(content);
            heap_caps_free(path);
            fclose(f);
            return -1;
        }
        
        FILE *out = fopen(full_path, "wb");
        if (out) {
            fwrite(content, 1, content_len, out);
            fclose(out);
        } else {
            ESP_LOGE(TAG, "Cannot create file: %s", full_path);
            heap_caps_free(content);
            heap_caps_free(path);
            fclose(f);
            return -1;
        }
        
        heap_caps_free(content);
        heap_caps_free(path);
    }
    
    fclose(f);
    ESP_LOGI(TAG, "Extracted %d files from EPP", file_count);
    return 0;
}

static int ends_with(const char *str, const char *suffix)
{
    int len_str = strlen(str);
    int len_suffix = strlen(suffix);
    if (len_str < len_suffix) return 0;
    return strcmp(str + len_str - len_suffix, suffix) == 0;
}

void app_installer_init(void)
{
    mkdir(APP_INSTALL_DIR, 0755);
    ESP_LOGI(TAG, "App installer initialized");
}

install_result_t app_installer_install(const char *src_path)
{
    ESP_LOGI(TAG, "Installing app from: %s", src_path);
    
    char app_path[512];
    if (ends_with(src_path, ".epp")) {
        char app_name[APP_NAME_MAX];
        const char *name_start = strrchr(src_path, '/');
        const char *name_end = strrchr(src_path, '.');
        if (name_start) name_start++;
        else name_start = src_path;
        
        int name_len = (name_end - name_start);
        if (name_len > APP_NAME_MAX - 1) name_len = APP_NAME_MAX - 1;
        strncpy(app_name, name_start, name_len);
        app_name[name_len] = '\0';
        
        snprintf(app_path, sizeof(app_path), "%s/%s", APP_INSTALL_DIR, app_name);
        mkdir(app_path, 0755);
        
        if (epp_extract(src_path, app_path) != 0) {
            return INSTALL_ERR_INVALID_MANIFEST;
        }
    } else {
        strncpy(app_path, src_path, sizeof(app_path) - 1);
        app_path[sizeof(app_path) - 1] = '\0';
    }
    
    app_info_t app = {0};
    install_result_t ret = read_manifest(app_path, &app);
    if (ret != INSTALL_OK) {
        return ret;
    }
    
    const app_info_t *existing = app_manager_find_app(app.name);
    if (existing) {
        ESP_LOGE(TAG, "App already exists: %s", app.name);
        return INSTALL_ERR_ALREADY_EXISTS;
    }
    
    strncpy(app.path, app_path, sizeof(app.path) - 1);
    
    if (app.data.dynamic.entry_point[0] == '\0') {
        strcpy(app.data.dynamic.entry_point, "layout.json");
    }
    
    app_manager_register(&app);
    ESP_LOGI(TAG, "App installed successfully: %s", app.name);
    
    return INSTALL_OK;
}

install_result_t app_installer_uninstall(const char *app_name)
{
    ESP_LOGI(TAG, "Uninstalling app: %s", app_name);
    
    const app_info_t *app = app_manager_find_app(app_name);
    if (!app) {
        return INSTALL_ERR_NOT_FOUND;
    }
    
    if (app->type == APP_TYPE_NATIVE) {
        ESP_LOGE(TAG, "Cannot uninstall native app: %s", app_name);
        return INSTALL_ERR_INVALID_MANIFEST;
    }
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", app->path);
    system(cmd);
    
    ESP_LOGI(TAG, "App uninstalled: %s", app_name);
    return INSTALL_OK;
}

int app_installer_list_installed(app_info_t *apps, int max_count)
{
    DIR *dir = opendir(APP_INSTALL_DIR);
    if (!dir) {
        ESP_LOGW(TAG, "Cannot open apps directory: %s", APP_INSTALL_DIR);
        return 0;
    }
    
    int count = 0;
    struct dirent *entry;
    
    while ((entry = readdir(dir)) != NULL && count < max_count) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && 
            strcmp(entry->d_name, "..") != 0) {
            
            char app_path[512];
            snprintf(app_path, sizeof(app_path), "%s/%s", APP_INSTALL_DIR, entry->d_name);
            
            install_result_t ret = read_manifest(app_path, &apps[count]);
            if (ret == INSTALL_OK) {
                strncpy(apps[count].path, app_path, sizeof(apps[count].path) - 1);
                count++;
            }
        }
    }
    
    closedir(dir);
    ESP_LOGI(TAG, "Found %d installed apps", count);
    
    return count;
}

void app_installer_scan_and_register(void)
{
    app_info_t apps[MAX_APPS];
    int count = app_installer_list_installed(apps, MAX_APPS);
    
    for (int i = 0; i < count; i++) {
        const app_info_t *existing = app_manager_find_app(apps[i].name);
        if (!existing) {
            app_manager_register(&apps[i]);
        }
    }
    
    ESP_LOGI(TAG, "Registered %d dynamic apps", count);
}
