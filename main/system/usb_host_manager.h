#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    USB_HOST_STATE_IDLE,
    USB_HOST_STATE_INITIALIZED,
    USB_HOST_STATE_MOUNTED,
    USB_HOST_STATE_ERROR
} usb_host_state_t;

typedef void (*usb_host_event_cb_t)(usb_host_state_t state, const char *mount_path);

esp_err_t usb_host_manager_init(void);
esp_err_t usb_host_manager_deinit(void);
usb_host_state_t usb_host_manager_get_state(void);
const char *usb_host_manager_get_mount_path(void);
void usb_host_manager_register_event_cb(usb_host_event_cb_t cb);
bool usb_host_manager_is_mounted(void);

#ifdef __cplusplus
}
#endif
