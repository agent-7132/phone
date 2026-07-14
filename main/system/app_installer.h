#pragma once

#include "app_manager.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_INSTALL_DIR "/spiffs/apps"
#define APP_MANIFEST_FILE "manifest.json"

typedef enum {
    INSTALL_OK,
    INSTALL_ERR_NOT_FOUND,
    INSTALL_ERR_INVALID_MANIFEST,
    INSTALL_ERR_ALREADY_EXISTS,
    INSTALL_ERR_STORAGE,
    INSTALL_ERR_MEMORY,
} install_result_t;

void app_installer_init(void);

install_result_t app_installer_install(const char *src_path);

install_result_t app_installer_uninstall(const char *app_name);

int app_installer_list_installed(app_info_t *apps, int max_count);

void app_installer_scan_and_register(void);

#ifdef __cplusplus
}
#endif
