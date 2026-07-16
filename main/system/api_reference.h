#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_PHONE_API_VERSION_MAJOR 1
#define ESP_PHONE_API_VERSION_MINOR 0
#define ESP_PHONE_API_VERSION_PATCH 0

#define ESP_PHONE_API_VERSION ((ESP_PHONE_API_VERSION_MAJOR << 16) | \
                               (ESP_PHONE_API_VERSION_MINOR << 8) | \
                               ESP_PHONE_API_VERSION_PATCH)

#define ESP_PHONE_API_VERSION_STRING "1.0.0"

#define MAX_APPS 20
#define APP_NAME_MAX 32
#define APP_TITLE_MAX 64
#define APP_VERSION_MAX 16
#define APP_AUTHOR_MAX 32
#define APP_DESC_MAX 128

#define MAX_NOTIFICATIONS 10
#define MAX_NOTIFICATION_TITLE 64
#define MAX_NOTIFICATION_MESSAGE 256

#define MAX_SENSORS 5
#define MAX_SENSOR_NAME 32

#define MAX_LANGUAGES 5
#define MAX_STRING_ID 200
#define MAX_STRING_LEN 256



#define SERVICE_MANAGER_MAX_SERVICES 10
#define SERVICE_MANAGER_MAX_DEPENDENCIES 5

#define PERMISSION_MANAGER_MAX_APPS 20
#define PERMISSION_NVS_NAMESPACE "perm_mgr"

#define MAX_AP_RECORDS 20

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

typedef enum {
    NET_STATE_DISCONNECTED,
    NET_STATE_CONNECTING,
    NET_STATE_CONNECTED,
    NET_STATE_FAILED
} net_state_t;

typedef enum {
    NET_TYPE_NONE,
    NET_TYPE_WIFI,
    NET_TYPE_ETHERNET
} net_type_t;

typedef struct {
    net_type_t type;
    net_state_t state;
    char ssid[32];
    char ip[16];
    int rssi;
} net_info_t;

typedef struct {
    char ssid[32];
    int rssi;
    int channel;
} net_ap_info_t;

typedef enum {
    BT_STATE_DISABLED,
    BT_STATE_IDLE,
    BT_STATE_SCANNING,
    BT_STATE_CONNECTING,
    BT_STATE_CONNECTED,
    BT_STATE_DISCONNECTING
} bt_state_t;

typedef struct {
    char name[64];
    uint8_t addr[6];
    int8_t rssi;
    uint8_t addr_type;
    uint8_t bond_status;
} bt_device_t;

typedef struct {
    uint8_t addr[6];
    char name[64];
} bt_bonded_device_t;

typedef void (*bt_state_cb_t)(bt_state_t state);
typedef void (*bt_device_found_cb_t)(bt_device_t *device);
typedef void (*bt_connection_cb_t)(bool connected, uint8_t *addr);
typedef void (*bt_scan_complete_cb_t)(void);

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

typedef enum {
    SERVICE_STATE_STOPPED,
    SERVICE_STATE_RUNNING,
    SERVICE_STATE_PAUSED,
    SERVICE_STATE_CRASHED,
    SERVICE_STATE_STARTING
} service_state_t;

typedef enum {
    SERVICE_PRIORITY_LOW = 1,
    SERVICE_PRIORITY_MEDIUM = 5,
    SERVICE_PRIORITY_HIGH = 10,
    SERVICE_PRIORITY_CRITICAL = 15
} service_priority_t;

typedef struct {
    char name[32];
    esp_err_t (*start)(void *arg);
    void (*stop)(void *arg);
    void (*pause)(void *arg);
    void (*resume)(void *arg);
    bool (*health_check)(void *arg);
    void *arg;
    service_state_t state;
    service_priority_t priority;
    TaskHandle_t task_handle;
    size_t stack_size;
    
    bool auto_restart;
    int restart_count;
    int max_restarts;
    uint32_t restart_delay_ms;
    
    uint32_t max_memory_bytes;
    uint32_t cpu_usage_percent;
    uint32_t health_check_interval_ms;
    
    char dependencies[SERVICE_MANAGER_MAX_DEPENDENCIES][32];
    int dependency_count;
    
    uint64_t start_time;
    uint64_t total_run_time;
    uint64_t last_health_check;
    
    bool resource_limited;
} service_info_t;

typedef void (*service_event_cb_t)(const char *name, service_state_t state, void *arg);

typedef enum {
    UI_ANIM_FADE_IN,
    UI_ANIM_FADE_OUT,
    UI_ANIM_SLIDE_LEFT,
    UI_ANIM_SLIDE_RIGHT,
    UI_ANIM_SLIDE_UP,
    UI_ANIM_SLIDE_DOWN,
    UI_ANIM_SCALE_IN,
    UI_ANIM_SCALE_OUT,
    UI_ANIM_BOUNCE_IN,
    UI_ANIM_ROTATE_IN
} ui_anim_type_t;

typedef enum {
    UI_ANIM_EASE_LINEAR,
    UI_ANIM_EASE_IN,
    UI_ANIM_EASE_OUT,
    UI_ANIM_EASE_IN_OUT
} ui_anim_ease_t;

typedef void (*ui_anim_done_cb_t)(lv_obj_t *obj);

typedef enum {
    UI_FEEDBACK_TYPE_INFO,
    UI_FEEDBACK_TYPE_SUCCESS,
    UI_FEEDBACK_TYPE_WARNING,
    UI_FEEDBACK_TYPE_ERROR
} ui_feedback_type_t;

typedef enum {
    SENSOR_TYPE_UNKNOWN,
    SENSOR_TYPE_TEMPERATURE,
    SENSOR_TYPE_HUMIDITY,
    SENSOR_TYPE_ACCELEROMETER,
    SENSOR_TYPE_GYROSCOPE,
    SENSOR_TYPE_PRESSURE,
    SENSOR_TYPE_LIGHT
} sensor_type_t;

typedef struct {
    float temperature;
    float humidity;
} sensor_env_data_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} sensor_accel_data_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} sensor_gyro_data_t;

typedef union {
    sensor_env_data_t env;
    sensor_accel_data_t accel;
    sensor_gyro_data_t gyro;
    float pressure;
    float light;
} sensor_data_t;

typedef struct sensor sensor_t;

struct sensor {
    char name[MAX_SENSOR_NAME];
    sensor_type_t type;
    bool enabled;
    bool (*init)(sensor_t *sensor);
    bool (*read)(sensor_t *sensor, sensor_data_t *data);
    void (*deinit)(sensor_t *sensor);
    void *priv_data;
};

typedef void (*sensor_data_cb_t)(sensor_type_t type, sensor_data_t *data);



typedef enum {
    NOTIFICATION_TYPE_INFO,
    NOTIFICATION_TYPE_WARNING,
    NOTIFICATION_TYPE_ERROR,
    NOTIFICATION_TYPE_SUCCESS
} notification_type_t;

typedef struct {
    char title[MAX_NOTIFICATION_TITLE];
    char message[MAX_NOTIFICATION_MESSAGE];
    notification_type_t type;
    uint32_t timestamp;
    bool read;
} notification_t;

typedef enum {
    GESTURE_TYPE_NONE,
    GESTURE_TYPE_SWIPE_LEFT,
    GESTURE_TYPE_SWIPE_RIGHT,
    GESTURE_TYPE_SWIPE_UP,
    GESTURE_TYPE_SWIPE_DOWN,
    GESTURE_TYPE_TAP,
    GESTURE_TYPE_DOUBLE_TAP,
    GESTURE_TYPE_LONG_PRESS,
    GESTURE_TYPE_PINCH_IN,
    GESTURE_TYPE_PINCH_OUT
} gesture_type_t;

typedef void (*gesture_callback_t)(gesture_type_t type, lv_point_t start, lv_point_t end);

typedef enum {
    THEME_MODE_DARK,
    THEME_MODE_LIGHT
} theme_mode_t;

typedef struct {
    lv_color_t bg_primary;
    lv_color_t bg_secondary;
    lv_color_t bg_card;
    lv_color_t bg_button;
    lv_color_t bg_button_pressed;
    lv_color_t text_primary;
    lv_color_t text_secondary;
    lv_color_t text_highlight;
    lv_color_t border;
    lv_color_t accent;
} theme_colors_t;

typedef enum {
    LANG_EN = 0,
    LANG_ZH = 1,
    LANG_JA = 2,
    LANG_KO = 3,
    LANG_FR = 4
} language_t;

typedef enum {
    STR_ID_APP_NAME_DISPLAY = 0,
    STR_ID_APP_NAME_TOUCH,
    STR_ID_APP_NAME_I2C,
    STR_ID_APP_NAME_SDCARD,
    STR_ID_APP_NAME_AUDIO,
    STR_ID_APP_NAME_SETTINGS,
    STR_ID_APP_NAME_INFO,
    STR_ID_APP_NAME_ENCRYPT,
    STR_ID_APP_NAME_FILE_BROWSER,
    STR_ID_APP_NAME_NETWORK,
    STR_ID_APP_NAME_CAMERA,
    STR_ID_APP_NAME_MUSIC,
    STR_ID_APP_NAME_ALBUM,
    STR_ID_APP_NAME_READER,
    STR_ID_APP_NAME_APP_STORE,
    STR_ID_APP_NAME_BT_SETTINGS,
    STR_ID_TITLE_WIFI_SETTINGS,
    STR_ID_TITLE_BLUETOOTH_SETTINGS,
    STR_ID_TITLE_SYSTEM_INFO,
    STR_ID_TITLE_CONTROL_CENTER,
    STR_ID_TITLE_NOTIFICATION_CENTER,
    STR_ID_BTN_CONNECT,
    STR_ID_BTN_DISCONNECT,
    STR_ID_BTN_SCAN,
    STR_ID_BTN_OK,
    STR_ID_BTN_CANCEL,
    STR_ID_BTN_BACK,
    STR_ID_BTN_DELETE,
    STR_ID_BTN_RENAME,
    STR_ID_BTN_INSTALL,
    STR_ID_BTN_UNINSTALL,
    STR_ID_BTN_UPDATE,
    STR_ID_BTN_DOWNLOAD,
    STR_ID_LABEL_SSID,
    STR_ID_LABEL_PASSWORD,
    STR_ID_LABEL_STATUS,
    STR_ID_LABEL_BATTERY,
    STR_ID_LABEL_VOLUME,
    STR_ID_LABEL_BRIGHTNESS,
    STR_ID_LABEL_TIME,
    STR_ID_LABEL_DATE,
    STR_ID_LABEL_MEMORY,
    STR_ID_LABEL_TEMPERATURE,
    STR_ID_LABEL_HUMIDITY,
    STR_ID_TEXT_CONNECTED,
    STR_ID_TEXT_CONNECTING,
    STR_ID_TEXT_DISCONNECTED,
    STR_ID_TEXT_FAILED,
    STR_ID_TEXT_SAVED_NETWORKS,
    STR_ID_TEXT_AVAILABLE_NETWORKS,
    STR_ID_TEXT_PAIRED_DEVICES,
    STR_ID_TEXT_SCANNING,
    STR_ID_TEXT_NO_DEVICES,
    STR_ID_TEXT_NO_NETWORKS,
    STR_ID_TEXT_BATTERY_CHARGING,
    STR_ID_TEXT_BATTERY_DISCHARGING,
    STR_ID_TEXT_LOW_BATTERY,
    STR_ID_TEXT_FILE_DELETE_CONFIRM,
    STR_ID_TEXT_FILE_RENAME,
    STR_ID_TEXT_APP_INSTALL_SUCCESS,
    STR_ID_TEXT_APP_INSTALL_FAILED,
    STR_ID_TEXT_OTA_UPDATE_AVAILABLE,
    STR_ID_TEXT_OTA_UPDATING,
    STR_ID_TEXT_OTA_COMPLETED,
    STR_ID_TEXT_NOTIFICATION,
    STR_ID_TEXT_WARNING,
    STR_ID_TEXT_ERROR,
    STR_ID_TEXT_SUCCESS,
    STR_ID_TEXT_SEARCH,
    STR_ID_TEXT_DEEP_SEARCH,
    STR_ID_TEXT_SORT_BY_NAME,
    STR_ID_TEXT_SORT_BY_SIZE,
    STR_ID_TEXT_SORT_BY_DATE,
    STR_ID_TEXT_WELCOME,
    STR_ID_TEXT_HOME,
    STR_ID_TEXT_APPS,
    STR_ID_TEXT_SETTINGS,
    STR_ID_TEXT_ABOUT,
    STR_ID_TEXT_VERSION,
    STR_ID_TEXT_MANUFACTURER,
    STR_ID_TEXT_MODEL,
    STR_ID_TEXT_SERIAL_NUMBER,
    STR_ID_TEXT_LANGUAGE,
    STR_ID_TEXT_THEME,
    STR_ID_TEXT_DARK_MODE,
    STR_ID_TEXT_LIGHT_MODE,
    STR_ID_TEXT_AUTO,
    STR_ID_TEXT_ON,
    STR_ID_TEXT_OFF,
    STR_ID_TEXT_ENABLE,
    STR_ID_TEXT_DISABLE,
    STR_ID_TEXT_CONFIRM,
    STR_ID_TEXT_CLOSE,
    STR_ID_TEXT_CLEAR,
    STR_ID_TEXT_SELECT,
    STR_ID_TEXT_OPTION,
    STR_ID_TEXT_VALUE,
    STR_ID_TEXT_PERCENT,
    STR_ID_TEXT_KB,
    STR_ID_TEXT_MB,
    STR_ID_TEXT_GB,
    STR_ID_TEXT_SECONDS,
    STR_ID_TEXT_MINUTES,
    STR_ID_TEXT_HOURS,
    STR_ID_TEXT_DAYS,
    STR_ID_TEXT_YEAR,
    STR_ID_TEXT_MONTH,
    STR_ID_TEXT_WEEK,
    STR_ID_TEXT_MONDAY,
    STR_ID_TEXT_TUESDAY,
    STR_ID_TEXT_WEDNESDAY,
    STR_ID_TEXT_THURSDAY,
    STR_ID_TEXT_FRIDAY,
    STR_ID_TEXT_SATURDAY,
    STR_ID_TEXT_SUNDAY,
    STR_ID_TEXT_JANUARY,
    STR_ID_TEXT_FEBRUARY,
    STR_ID_TEXT_MARCH,
    STR_ID_TEXT_APRIL,
    STR_ID_TEXT_MAY,
    STR_ID_TEXT_JUNE,
    STR_ID_TEXT_JULY,
    STR_ID_TEXT_AUGUST,
    STR_ID_TEXT_SEPTEMBER,
    STR_ID_TEXT_OCTOBER,
    STR_ID_TEXT_NOVEMBER,
    STR_ID_TEXT_DECEMBER,
    STR_ID_LABEL_ACCELEROMETER,
    STR_ID_LABEL_SENSOR_DATA,
    STR_ID_TEXT_SENSOR_NOT_FOUND,
    STR_ID_TEXT_SENSOR_READING,
    STR_ID_TEXT_SENSOR_ERROR,
    STR_ID_TEXT_LANG_ENGLISH,
    STR_ID_TEXT_LANG_CHINESE,
    STR_ID_TEXT_LANG_JAPANESE,
    STR_ID_TEXT_LANG_KOREAN,
    STR_ID_TEXT_LANG_FRENCH,
    STR_ID_TEXT_SELECT_LANGUAGE,
    STR_ID_TEXT_CURRENT_LANGUAGE,
    STR_ID_TEXT_LANGUAGE_CHANGED,
    STR_ID_MAX
} string_id_t;



typedef struct {
    lv_obj_t *bar;
    lv_obj_t *clock_label;
    lv_obj_t *memory_label;
    lv_obj_t *sensor_label;
    lv_obj_t *net_icon;
    lv_obj_t *bt_icon;
    lv_obj_t *battery_icon;
    lv_obj_t *notification_icon;
} status_bar_t;

void app_manager_init(void);
void app_manager_register(const app_info_t *app);
void app_manager_open_app(const char *name);
void app_manager_go_home(void);
void app_manager_go_back(void);
int app_manager_get_app_count(void);
const app_info_t *app_manager_get_app(int index);
const app_info_t *app_manager_find_app(const char *name);

esp_err_t net_manager_init(void);
esp_err_t net_manager_deinit(void);
esp_err_t net_manager_wifi_connect(const char *ssid, const char *password);
esp_err_t net_manager_wifi_disconnect(void);
int net_manager_wifi_scan(net_ap_info_t *ap_list, int max_count);
esp_err_t net_manager_ethernet_start(void);
esp_err_t net_manager_ethernet_stop(void);
net_info_t *net_manager_get_info(void);
void net_manager_register_callback(void (*cb)(net_info_t *info));

esp_err_t bt_manager_init(void);
esp_err_t bt_manager_enable(void);
esp_err_t bt_manager_disable(void);
bt_state_t bt_manager_get_state(void);
esp_err_t bt_manager_start_scan(void);
esp_err_t bt_manager_stop_scan(void);
esp_err_t bt_manager_connect(const uint8_t *addr, uint8_t addr_type);
esp_err_t bt_manager_disconnect(void);
esp_err_t bt_manager_remove_bond(const uint8_t *addr);
int bt_manager_get_bonded_devices(bt_bonded_device_t *devices, int max_count);
void bt_manager_register_state_cb(bt_state_cb_t cb);
void bt_manager_register_device_found_cb(bt_device_found_cb_t cb);
void bt_manager_register_connection_cb(bt_connection_cb_t cb);
void bt_manager_register_scan_complete_cb(bt_scan_complete_cb_t cb);
void bt_manager_notify_battery_level(uint8_t level);

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

esp_err_t service_manager_init(void);
esp_err_t service_manager_register(const char *name, 
                                   esp_err_t (*start)(void *arg),
                                   void (*stop)(void *arg),
                                   void (*pause)(void *arg),
                                   void (*resume)(void *arg),
                                   bool (*health_check)(void *arg),
                                   service_priority_t priority, 
                                   size_t stack_size,
                                   void *arg);
esp_err_t service_manager_unregister(const char *name);
esp_err_t service_manager_start(const char *name);
esp_err_t service_manager_stop(const char *name);
esp_err_t service_manager_pause(const char *name);
esp_err_t service_manager_resume(const char *name);
service_state_t service_manager_get_state(const char *name);
service_info_t *service_manager_get_service(const char *name);
service_info_t *service_manager_get_service_by_index(int index);
int service_manager_get_service_count(void);
void service_manager_dump_services(void);
esp_err_t service_manager_add_dependency(const char *name, const char *dep_name);
esp_err_t service_manager_set_auto_restart(const char *name, bool enable, int max_restarts, uint32_t delay_ms);
esp_err_t service_manager_set_resource_limits(const char *name, uint32_t max_memory_bytes, uint32_t cpu_usage_percent);
esp_err_t service_manager_set_health_check(const char *name, bool (*health_check)(void *arg), uint32_t interval_ms);
void service_manager_pause_low_priority(int min_priority);
void service_manager_resume_all(void);
void service_manager_register_event_callback(service_event_cb_t cb, void *arg);
void service_manager_run_health_checks(void);

esp_err_t ui_animation_init(void);
void ui_animation_start(lv_obj_t *obj, ui_anim_type_t type, uint32_t duration, 
                        ui_anim_ease_t ease, ui_anim_done_cb_t cb);
void ui_animation_fade(lv_obj_t *obj, bool fade_in, uint32_t duration);
void ui_animation_slide(lv_obj_t *obj, lv_dir_t dir, uint32_t duration);
void ui_animation_scale(lv_obj_t *obj, bool scale_in, uint32_t duration);
void ui_animation_bounce(lv_obj_t *obj, bool bounce_in, uint32_t duration);
void ui_animation_rotate(lv_obj_t *obj, bool rotate_in, uint32_t duration);
void ui_animation_stop(lv_obj_t *obj);
void ui_animation_set_default_duration(uint32_t duration);
void ui_animation_set_default_ease(ui_anim_ease_t ease);
uint32_t ui_animation_get_default_duration(void);
ui_anim_ease_t ui_animation_get_default_ease(void);

esp_err_t ui_feedback_init(void);
void ui_feedback_show_toast(const char *title, const char *message, ui_feedback_type_t type, uint32_t duration_ms);
void ui_feedback_show_loading(lv_obj_t *parent, const char *text);
void ui_feedback_hide_loading(void);
void ui_feedback_show_error(const char *title, const char *message);
void ui_feedback_button_press(lv_obj_t *btn);

esp_err_t sensor_manager_init(void);
esp_err_t sensor_manager_register(sensor_t *sensor);
esp_err_t sensor_manager_unregister(const char *name);
sensor_t *sensor_manager_find(const char *name);
int sensor_manager_get_sensor_count(void);
sensor_t *sensor_manager_get_sensor(int index);
esp_err_t sensor_manager_set_enabled(const char *name, bool enabled);
esp_err_t sensor_manager_read(const char *name, sensor_data_t *data);
esp_err_t sensor_manager_read_all(void);
void sensor_manager_register_data_cb(sensor_data_cb_t cb);
bool sensor_sht3x_init(sensor_t *sensor);
bool sensor_sht3x_read(sensor_t *sensor, sensor_data_t *data);
void sensor_sht3x_deinit(sensor_t *sensor);
bool sensor_lis3dh_init(sensor_t *sensor);
bool sensor_lis3dh_read(sensor_t *sensor, sensor_data_t *data);
void sensor_lis3dh_deinit(sensor_t *sensor);

esp_err_t notification_manager_init(void);
void notification_manager_add(const char *title, const char *message, notification_type_t type);
void notification_manager_remove(int index);
void notification_manager_clear_all(void);
int notification_manager_get_count(void);
int notification_manager_get_unread_count(void);
const notification_t *notification_manager_get(int index);
void notification_manager_mark_read(int index);
void notification_manager_show_notification_center(lv_obj_t *parent);
void notification_manager_hide_notification_center(void);
lv_obj_t *notification_manager_get_center(void);

esp_err_t gesture_manager_init(void);
void gesture_manager_register_callback(gesture_callback_t cb);
void gesture_manager_unregister_callback(gesture_callback_t cb);
void gesture_manager_set_enabled(bool enabled);
bool gesture_manager_is_enabled(void);

esp_err_t theme_manager_init(void);
void theme_manager_set_mode(theme_mode_t mode);
theme_mode_t theme_manager_get_mode(void);
void theme_manager_apply(lv_obj_t *obj);
void theme_manager_apply_global(void);
const theme_colors_t *theme_manager_get_colors(void);

esp_err_t i18n_manager_init(void);
esp_err_t i18n_manager_load_language(language_t lang);
language_t i18n_manager_get_current_language(void);
esp_err_t i18n_manager_set_language(language_t lang);
const char *i18n_manager_get_string(string_id_t id);
esp_err_t i18n_manager_save_language(void);
esp_err_t i18n_manager_load_saved_language(void);
const char *i18n_manager_get_language_name(language_t lang);
int i18n_manager_get_language_count(void);
language_t i18n_manager_get_language_by_index(int index);

status_bar_t *status_bar_create(lv_obj_t *parent);
void status_bar_update_clock(void);
void status_bar_update_memory(void);
void status_bar_set_sensor_data(float temperature, float humidity);
void status_bar_set_network(int type, bool connected);
void status_bar_set_bluetooth(bool enabled, bool connected);
void status_bar_set_notification(int count);

#define UTILS_HASH_TABLE_SIZE 31

typedef struct hash_node {
    const char *key;
    void *value;
    struct hash_node *next;
} hash_node_t;

typedef struct {
    hash_node_t *buckets[UTILS_HASH_TABLE_SIZE];
    int count;
} hash_table_t;

uint32_t utils_hash_string(const char *str);
void utils_hash_table_init(hash_table_t *table);
void utils_hash_table_insert(hash_table_t *table, const char *key, void *value);
void *utils_hash_table_lookup(hash_table_t *table, const char *key);
bool utils_hash_table_remove(hash_table_t *table, const char *key);
void utils_hash_table_clear(hash_table_t *table);
int utils_hash_table_count(hash_table_t *table);
int utils_strcasecmp(const char *s1, const char *s2);
char *utils_strcasestr(const char *haystack, const char *needle);
char *utils_strdup(const char *str);
void utils_sort_string_array(char **array, int count);
int utils_binary_search_string(char **array, int count, const char *key);

#define UI_UTILS_COLOR_PRIMARY_BG 0x1a1a2e
#define UI_UTILS_COLOR_SECONDARY_BG 0x252540
#define UI_UTILS_COLOR_CARD_BG 0x252540
#define UI_UTILS_COLOR_BUTTON_BG 0x3a3a5a
#define UI_UTILS_COLOR_BUTTON_PRESSED 0x4a4a6a
#define UI_UTILS_COLOR_BUTTON_ACTIVE 0x4a4a6a
#define UI_UTILS_COLOR_TEXT_PRIMARY 0xFFFFFF
#define UI_UTILS_COLOR_TEXT_SECONDARY 0xAAAAAA
#define UI_UTILS_COLOR_BORDER 0x3a3a5a
#define UI_UTILS_COLOR_ACCENT 0x6c5ce7

#define UI_UTILS_FONT_DEFAULT &lv_font_montserrat_14
#define UI_UTILS_FONT_TITLE &lv_font_montserrat_14
#define UI_UTILS_FONT_SMALL &lv_font_montserrat_14

#define UI_UTILS_BORDER_WIDTH 1
#define UI_UTILS_RADIUS_DEFAULT 10
#define UI_UTILS_RADIUS_BUTTON 8
#define UI_UTILS_RADIUS_CARD 10

#define UI_UTILS_PADDING_SMALL 5
#define UI_UTILS_PADDING_DEFAULT 10
#define UI_UTILS_PADDING_LARGE 15

typedef struct {
    lv_color_t bg_color;
    lv_color_t border_color;
    uint8_t border_width;
    uint8_t radius;
    uint8_t padding;
} ui_container_style_t;

typedef struct {
    lv_color_t normal_color;
    lv_color_t pressed_color;
    lv_color_t active_color;
    lv_color_t text_color;
    const lv_font_t *font;
    uint8_t radius;
    uint16_t width;
    uint16_t height;
} ui_button_style_t;

lv_obj_t *ui_utils_create_screen(void);
lv_obj_t *ui_utils_create_container(lv_obj_t *parent, uint16_t width, uint16_t height,
                                    const ui_container_style_t *style);
lv_obj_t *ui_utils_create_container_default(lv_obj_t *parent, uint16_t width, uint16_t height);
lv_obj_t *ui_utils_create_button(lv_obj_t *parent, const char *text,
                                 lv_event_cb_t callback, void *user_data,
                                 const ui_button_style_t *style);
lv_obj_t *ui_utils_create_button_default(lv_obj_t *parent, const char *text,
                                         lv_event_cb_t callback, void *user_data);
lv_obj_t *ui_utils_create_back_button(lv_obj_t *parent);
lv_obj_t *ui_utils_create_title(lv_obj_t *parent, const char *text);
lv_obj_t *ui_utils_create_label(lv_obj_t *parent, const char *text,
                                const lv_font_t *font, lv_color_t color);
lv_obj_t *ui_utils_create_label_default(lv_obj_t *parent, const char *text);
void ui_utils_set_button_text(lv_obj_t *btn, const char *text);
void ui_utils_set_button_style(lv_obj_t *btn, const ui_button_style_t *style);
void ui_utils_center_label(lv_obj_t *btn);

#ifdef __cplusplus
}
#endif