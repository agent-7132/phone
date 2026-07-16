#include "i18n_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "I18N_MANAGER";

static language_t current_lang = LANG_EN;

static const char *en_strings[STR_ID_MAX] = {
    [STR_ID_APP_NAME_DISPLAY] = "Display",
    [STR_ID_APP_NAME_TOUCH] = "Touch",
    [STR_ID_APP_NAME_I2C] = "I2C Scan",
    [STR_ID_APP_NAME_SDCARD] = "SD Card",
    [STR_ID_APP_NAME_AUDIO] = "Audio",
    [STR_ID_APP_NAME_SETTINGS] = "Settings",
    [STR_ID_APP_NAME_INFO] = "System Info",
    [STR_ID_APP_NAME_ENCRYPT] = "Encrypt",
    [STR_ID_APP_NAME_FILE_BROWSER] = "File Browser",
    [STR_ID_APP_NAME_NETWORK] = "Network",
    [STR_ID_APP_NAME_CAMERA] = "Camera",
    [STR_ID_APP_NAME_MUSIC] = "Music",
    [STR_ID_APP_NAME_ALBUM] = "Album",
    [STR_ID_APP_NAME_READER] = "Reader",
    [STR_ID_APP_NAME_APP_STORE] = "App Store",
    [STR_ID_APP_NAME_BT_SETTINGS] = "Bluetooth",
    [STR_ID_TITLE_WIFI_SETTINGS] = "WiFi Settings",
    [STR_ID_TITLE_BLUETOOTH_SETTINGS] = "Bluetooth Settings",
    [STR_ID_TITLE_SYSTEM_INFO] = "System Info",
    [STR_ID_TITLE_CONTROL_CENTER] = "Control Center",
    [STR_ID_TITLE_NOTIFICATION_CENTER] = "Notification Center",
    [STR_ID_BTN_CONNECT] = "Connect",
    [STR_ID_BTN_DISCONNECT] = "Disconnect",
    [STR_ID_BTN_SCAN] = "Scan",
    [STR_ID_BTN_OK] = "OK",
    [STR_ID_BTN_CANCEL] = "Cancel",
    [STR_ID_BTN_BACK] = "Back",
    [STR_ID_BTN_DELETE] = "Delete",
    [STR_ID_BTN_RENAME] = "Rename",
    [STR_ID_BTN_INSTALL] = "Install",
    [STR_ID_BTN_UNINSTALL] = "Uninstall",
    [STR_ID_BTN_UPDATE] = "Update",
    [STR_ID_BTN_DOWNLOAD] = "Download",
    [STR_ID_LABEL_SSID] = "SSID",
    [STR_ID_LABEL_PASSWORD] = "Password",
    [STR_ID_LABEL_STATUS] = "Status",
    [STR_ID_LABEL_VOLUME] = "Volume",
    [STR_ID_LABEL_BRIGHTNESS] = "Brightness",
    [STR_ID_LABEL_TIME] = "Time",
    [STR_ID_LABEL_DATE] = "Date",
    [STR_ID_LABEL_MEMORY] = "Memory",
    [STR_ID_LABEL_TEMPERATURE] = "Temperature",
    [STR_ID_LABEL_HUMIDITY] = "Humidity",
    [STR_ID_TEXT_CONNECTED] = "Connected",
    [STR_ID_TEXT_CONNECTING] = "Connecting...",
    [STR_ID_TEXT_DISCONNECTED] = "Disconnected",
    [STR_ID_TEXT_FAILED] = "Failed",
    [STR_ID_TEXT_SAVED_NETWORKS] = "Saved Networks",
    [STR_ID_TEXT_AVAILABLE_NETWORKS] = "Available Networks",
    [STR_ID_TEXT_PAIRED_DEVICES] = "Paired Devices",
    [STR_ID_TEXT_SCANNING] = "Scanning...",
    [STR_ID_TEXT_NO_DEVICES] = "No devices found",
    [STR_ID_TEXT_NO_NETWORKS] = "No networks found",
    [STR_ID_TEXT_FILE_DELETE_CONFIRM] = "Are you sure you want to delete this file?",
    [STR_ID_TEXT_FILE_RENAME] = "New name:",
    [STR_ID_TEXT_APP_INSTALL_SUCCESS] = "App installed successfully!",
    [STR_ID_TEXT_APP_INSTALL_FAILED] = "Failed to install app",
    [STR_ID_TEXT_OTA_UPDATE_AVAILABLE] = "Update available",
    [STR_ID_TEXT_OTA_UPDATING] = "Updating...",
    [STR_ID_TEXT_OTA_COMPLETED] = "Update completed",
    [STR_ID_TEXT_NOTIFICATION] = "Notification",
    [STR_ID_TEXT_WARNING] = "Warning",
    [STR_ID_TEXT_ERROR] = "Error",
    [STR_ID_TEXT_SUCCESS] = "Success",
    [STR_ID_TEXT_SEARCH] = "Search...",
    [STR_ID_TEXT_DEEP_SEARCH] = "Deep Search",
    [STR_ID_TEXT_SORT_BY_NAME] = "Sort by Name",
    [STR_ID_TEXT_SORT_BY_SIZE] = "Sort by Size",
    [STR_ID_TEXT_SORT_BY_DATE] = "Sort by Date",
    [STR_ID_TEXT_WELCOME] = "Welcome",
    [STR_ID_TEXT_HOME] = "Home",
    [STR_ID_TEXT_APPS] = "Apps",
    [STR_ID_TEXT_SETTINGS] = "Settings",
    [STR_ID_TEXT_ABOUT] = "About",
    [STR_ID_TEXT_VERSION] = "Version",
    [STR_ID_TEXT_MANUFACTURER] = "Manufacturer",
    [STR_ID_TEXT_MODEL] = "Model",
    [STR_ID_TEXT_SERIAL_NUMBER] = "Serial Number",
    [STR_ID_TEXT_LANGUAGE] = "Language",
    [STR_ID_TEXT_THEME] = "Theme",
    [STR_ID_TEXT_DARK_MODE] = "Dark Mode",
    [STR_ID_TEXT_LIGHT_MODE] = "Light Mode",
    [STR_ID_TEXT_AUTO] = "Auto",
    [STR_ID_TEXT_ON] = "On",
    [STR_ID_TEXT_OFF] = "Off",
    [STR_ID_TEXT_ENABLE] = "Enable",
    [STR_ID_TEXT_DISABLE] = "Disable",
    [STR_ID_TEXT_CONFIRM] = "Confirm",
    [STR_ID_TEXT_CLOSE] = "Close",
    [STR_ID_TEXT_CLEAR] = "Clear",
    [STR_ID_TEXT_SELECT] = "Select",
    [STR_ID_TEXT_OPTION] = "Option",
    [STR_ID_TEXT_VALUE] = "Value",
    [STR_ID_TEXT_PERCENT] = "%",
    [STR_ID_TEXT_KB] = "KB",
    [STR_ID_TEXT_MB] = "MB",
    [STR_ID_TEXT_GB] = "GB",
    [STR_ID_TEXT_SECONDS] = "seconds",
    [STR_ID_TEXT_MINUTES] = "minutes",
    [STR_ID_TEXT_HOURS] = "hours",
    [STR_ID_TEXT_DAYS] = "days",
    [STR_ID_TEXT_YEAR] = "Year",
    [STR_ID_TEXT_MONTH] = "Month",
    [STR_ID_TEXT_WEEK] = "Week",
    [STR_ID_TEXT_MONDAY] = "Monday",
    [STR_ID_TEXT_TUESDAY] = "Tuesday",
    [STR_ID_TEXT_WEDNESDAY] = "Wednesday",
    [STR_ID_TEXT_THURSDAY] = "Thursday",
    [STR_ID_TEXT_FRIDAY] = "Friday",
    [STR_ID_TEXT_SATURDAY] = "Saturday",
    [STR_ID_TEXT_SUNDAY] = "Sunday",
    [STR_ID_TEXT_JANUARY] = "January",
    [STR_ID_TEXT_FEBRUARY] = "February",
    [STR_ID_TEXT_MARCH] = "March",
    [STR_ID_TEXT_APRIL] = "April",
    [STR_ID_TEXT_MAY] = "May",
    [STR_ID_TEXT_JUNE] = "June",
    [STR_ID_TEXT_JULY] = "July",
    [STR_ID_TEXT_AUGUST] = "August",
    [STR_ID_TEXT_SEPTEMBER] = "September",
    [STR_ID_TEXT_OCTOBER] = "October",
    [STR_ID_TEXT_NOVEMBER] = "November",
    [STR_ID_TEXT_DECEMBER] = "December",
    [STR_ID_LABEL_ACCELEROMETER] = "Accelerometer",
    [STR_ID_LABEL_SENSOR_DATA] = "Sensor Data",
    [STR_ID_TEXT_SENSOR_NOT_FOUND] = "Sensor not found",
    [STR_ID_TEXT_SENSOR_READING] = "Reading...",
    [STR_ID_TEXT_SENSOR_ERROR] = "Read error",
    [STR_ID_TEXT_LANG_ENGLISH] = "English",
    [STR_ID_TEXT_LANG_CHINESE] = "中文",
    [STR_ID_TEXT_LANG_JAPANESE] = "日本語",
    [STR_ID_TEXT_LANG_KOREAN] = "한국어",
    [STR_ID_TEXT_LANG_FRENCH] = "Français",
    [STR_ID_TEXT_SELECT_LANGUAGE] = "Select Language",
    [STR_ID_TEXT_CURRENT_LANGUAGE] = "Current Language",
    [STR_ID_TEXT_LANGUAGE_CHANGED] = "Language changed",
};

static const char *zh_strings[STR_ID_MAX] = {
    [STR_ID_APP_NAME_DISPLAY] = "显示",
    [STR_ID_APP_NAME_TOUCH] = "触摸",
    [STR_ID_APP_NAME_I2C] = "I2C扫描",
    [STR_ID_APP_NAME_SDCARD] = "SD卡",
    [STR_ID_APP_NAME_AUDIO] = "音频",
    [STR_ID_APP_NAME_SETTINGS] = "设置",
    [STR_ID_APP_NAME_INFO] = "系统信息",
    [STR_ID_APP_NAME_ENCRYPT] = "加密",
    [STR_ID_APP_NAME_FILE_BROWSER] = "文件浏览器",
    [STR_ID_APP_NAME_NETWORK] = "网络",
    [STR_ID_APP_NAME_CAMERA] = "相机",
    [STR_ID_APP_NAME_MUSIC] = "音乐",
    [STR_ID_APP_NAME_ALBUM] = "相册",
    [STR_ID_APP_NAME_READER] = "阅读器",
    [STR_ID_APP_NAME_APP_STORE] = "应用商店",
    [STR_ID_APP_NAME_BT_SETTINGS] = "蓝牙",
    [STR_ID_TITLE_WIFI_SETTINGS] = "WiFi设置",
    [STR_ID_TITLE_BLUETOOTH_SETTINGS] = "蓝牙设置",
    [STR_ID_TITLE_SYSTEM_INFO] = "系统信息",
    [STR_ID_TITLE_CONTROL_CENTER] = "控制中心",
    [STR_ID_TITLE_NOTIFICATION_CENTER] = "通知中心",
    [STR_ID_BTN_CONNECT] = "连接",
    [STR_ID_BTN_DISCONNECT] = "断开",
    [STR_ID_BTN_SCAN] = "扫描",
    [STR_ID_BTN_OK] = "确定",
    [STR_ID_BTN_CANCEL] = "取消",
    [STR_ID_BTN_BACK] = "返回",
    [STR_ID_BTN_DELETE] = "删除",
    [STR_ID_BTN_RENAME] = "重命名",
    [STR_ID_BTN_INSTALL] = "安装",
    [STR_ID_BTN_UNINSTALL] = "卸载",
    [STR_ID_BTN_UPDATE] = "更新",
    [STR_ID_BTN_DOWNLOAD] = "下载",
    [STR_ID_LABEL_SSID] = "WiFi名称",
    [STR_ID_LABEL_PASSWORD] = "密码",
    [STR_ID_LABEL_STATUS] = "状态",
    [STR_ID_LABEL_VOLUME] = "音量",
    [STR_ID_LABEL_BRIGHTNESS] = "亮度",
    [STR_ID_LABEL_TIME] = "时间",
    [STR_ID_LABEL_DATE] = "日期",
    [STR_ID_LABEL_MEMORY] = "内存",
    [STR_ID_LABEL_TEMPERATURE] = "温度",
    [STR_ID_LABEL_HUMIDITY] = "湿度",
    [STR_ID_TEXT_CONNECTED] = "已连接",
    [STR_ID_TEXT_CONNECTING] = "连接中...",
    [STR_ID_TEXT_DISCONNECTED] = "未连接",
    [STR_ID_TEXT_FAILED] = "失败",
    [STR_ID_TEXT_SAVED_NETWORKS] = "已保存网络",
    [STR_ID_TEXT_AVAILABLE_NETWORKS] = "可用网络",
    [STR_ID_TEXT_PAIRED_DEVICES] = "已配对设备",
    [STR_ID_TEXT_SCANNING] = "扫描中...",
    [STR_ID_TEXT_NO_DEVICES] = "未发现设备",
    [STR_ID_TEXT_NO_NETWORKS] = "未发现网络",
    [STR_ID_TEXT_FILE_DELETE_CONFIRM] = "确定要删除此文件吗？",
    [STR_ID_TEXT_FILE_RENAME] = "新名称：",
    [STR_ID_TEXT_APP_INSTALL_SUCCESS] = "应用安装成功！",
    [STR_ID_TEXT_APP_INSTALL_FAILED] = "应用安装失败",
    [STR_ID_TEXT_OTA_UPDATE_AVAILABLE] = "有可用更新",
    [STR_ID_TEXT_OTA_UPDATING] = "更新中...",
    [STR_ID_TEXT_OTA_COMPLETED] = "更新完成",
    [STR_ID_TEXT_NOTIFICATION] = "通知",
    [STR_ID_TEXT_WARNING] = "警告",
    [STR_ID_TEXT_ERROR] = "错误",
    [STR_ID_TEXT_SUCCESS] = "成功",
    [STR_ID_TEXT_SEARCH] = "搜索...",
    [STR_ID_TEXT_DEEP_SEARCH] = "深度搜索",
    [STR_ID_TEXT_SORT_BY_NAME] = "按名称排序",
    [STR_ID_TEXT_SORT_BY_SIZE] = "按大小排序",
    [STR_ID_TEXT_SORT_BY_DATE] = "按日期排序",
    [STR_ID_TEXT_WELCOME] = "欢迎",
    [STR_ID_TEXT_HOME] = "主页",
    [STR_ID_TEXT_APPS] = "应用",
    [STR_ID_TEXT_SETTINGS] = "设置",
    [STR_ID_TEXT_ABOUT] = "关于",
    [STR_ID_TEXT_VERSION] = "版本",
    [STR_ID_TEXT_MANUFACTURER] = "制造商",
    [STR_ID_TEXT_MODEL] = "型号",
    [STR_ID_TEXT_SERIAL_NUMBER] = "序列号",
    [STR_ID_TEXT_LANGUAGE] = "语言",
    [STR_ID_TEXT_THEME] = "主题",
    [STR_ID_TEXT_DARK_MODE] = "深色模式",
    [STR_ID_TEXT_LIGHT_MODE] = "浅色模式",
    [STR_ID_TEXT_AUTO] = "自动",
    [STR_ID_TEXT_ON] = "开",
    [STR_ID_TEXT_OFF] = "关",
    [STR_ID_TEXT_ENABLE] = "启用",
    [STR_ID_TEXT_DISABLE] = "禁用",
    [STR_ID_TEXT_CONFIRM] = "确认",
    [STR_ID_TEXT_CLOSE] = "关闭",
    [STR_ID_TEXT_CLEAR] = "清除",
    [STR_ID_TEXT_SELECT] = "选择",
    [STR_ID_TEXT_OPTION] = "选项",
    [STR_ID_TEXT_VALUE] = "值",
    [STR_ID_TEXT_PERCENT] = "%",
    [STR_ID_TEXT_KB] = "KB",
    [STR_ID_TEXT_MB] = "MB",
    [STR_ID_TEXT_GB] = "GB",
    [STR_ID_TEXT_SECONDS] = "秒",
    [STR_ID_TEXT_MINUTES] = "分钟",
    [STR_ID_TEXT_HOURS] = "小时",
    [STR_ID_TEXT_DAYS] = "天",
    [STR_ID_TEXT_YEAR] = "年",
    [STR_ID_TEXT_MONTH] = "月",
    [STR_ID_TEXT_WEEK] = "周",
    [STR_ID_TEXT_MONDAY] = "周一",
    [STR_ID_TEXT_TUESDAY] = "周二",
    [STR_ID_TEXT_WEDNESDAY] = "周三",
    [STR_ID_TEXT_THURSDAY] = "周四",
    [STR_ID_TEXT_FRIDAY] = "周五",
    [STR_ID_TEXT_SATURDAY] = "周六",
    [STR_ID_TEXT_SUNDAY] = "周日",
    [STR_ID_TEXT_JANUARY] = "一月",
    [STR_ID_TEXT_FEBRUARY] = "二月",
    [STR_ID_TEXT_MARCH] = "三月",
    [STR_ID_TEXT_APRIL] = "四月",
    [STR_ID_TEXT_MAY] = "五月",
    [STR_ID_TEXT_JUNE] = "六月",
    [STR_ID_TEXT_JULY] = "七月",
    [STR_ID_TEXT_AUGUST] = "八月",
    [STR_ID_TEXT_SEPTEMBER] = "九月",
    [STR_ID_TEXT_OCTOBER] = "十月",
    [STR_ID_TEXT_NOVEMBER] = "十一月",
    [STR_ID_TEXT_DECEMBER] = "十二月",
    [STR_ID_LABEL_ACCELEROMETER] = "加速度计",
    [STR_ID_LABEL_SENSOR_DATA] = "传感器数据",
    [STR_ID_TEXT_SENSOR_NOT_FOUND] = "未找到传感器",
    [STR_ID_TEXT_SENSOR_READING] = "读取中...",
    [STR_ID_TEXT_SENSOR_ERROR] = "读取错误",
    [STR_ID_TEXT_LANG_ENGLISH] = "英语",
    [STR_ID_TEXT_LANG_CHINESE] = "中文",
    [STR_ID_TEXT_LANG_JAPANESE] = "日语",
    [STR_ID_TEXT_LANG_KOREAN] = "韩语",
    [STR_ID_TEXT_LANG_FRENCH] = "法语",
    [STR_ID_TEXT_SELECT_LANGUAGE] = "选择语言",
    [STR_ID_TEXT_CURRENT_LANGUAGE] = "当前语言",
    [STR_ID_TEXT_LANGUAGE_CHANGED] = "语言已更改",
};

static const char *(*current_strings)[STR_ID_MAX] = &en_strings;

static const char *lang_names[MAX_LANGUAGES] = {
    [LANG_EN] = "English",
    [LANG_ZH] = "中文",
    [LANG_JA] = "日本語",
    [LANG_KO] = "한국어",
    [LANG_FR] = "Français",
};

static esp_err_t i18n_manager_load_language_table(language_t lang)
{
    switch (lang) {
    case LANG_EN:
        current_strings = &en_strings;
        break;
    case LANG_ZH:
        current_strings = &zh_strings;
        break;
    case LANG_JA:
        current_strings = &en_strings;
        break;
    case LANG_KO:
        current_strings = &en_strings;
        break;
    case LANG_FR:
        current_strings = &en_strings;
        break;
    default:
        current_strings = &en_strings;
        break;
    }

    ESP_LOGI(TAG, "Language set to: %s", lang_names[lang]);

    return ESP_OK;
}

esp_err_t i18n_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing i18n manager...");

    i18n_manager_load_saved_language();

    return ESP_OK;
}

esp_err_t i18n_manager_load_language(language_t lang)
{
    return i18n_manager_set_language(lang);
}

language_t i18n_manager_get_current_language(void)
{
    return current_lang;
}

esp_err_t i18n_manager_set_language(language_t lang)
{
    if (lang >= MAX_LANGUAGES) {
        ESP_LOGE(TAG, "Invalid language: %d", lang);
        return ESP_ERR_INVALID_ARG;
    }

    current_lang = lang;
    i18n_manager_load_language_table(lang);
    i18n_manager_save_language();

    return ESP_OK;
}

const char *i18n_manager_get_string(string_id_t id)
{
    if (id >= STR_ID_MAX) {
        ESP_LOGE(TAG, "Invalid string ID: %d", id);
        return "";
    }

    return (*current_strings)[id];
}

esp_err_t i18n_manager_save_language(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open("settings", NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_u8(handle, "language", current_lang);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save language: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }

    ret = nvs_commit(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }

    nvs_close(handle);
    ESP_LOGI(TAG, "Language saved: %d", current_lang);

    return ESP_OK;
}

esp_err_t i18n_manager_load_saved_language(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open("settings", NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS, using default language: %s", esp_err_to_name(ret));
        current_lang = LANG_EN;
        i18n_manager_load_language_table(LANG_EN);
        return ret;
    }

    uint8_t lang;
    ret = nvs_get_u8(handle, "language", &lang);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to load language: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }

    nvs_close(handle);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        current_lang = LANG_EN;
    } else {
        current_lang = (language_t)lang;
    }

    i18n_manager_load_language_table(current_lang);

    ESP_LOGI(TAG, "Loaded language: %d", current_lang);

    return ESP_OK;
}

const char *i18n_manager_get_language_name(language_t lang)
{
    if (lang >= MAX_LANGUAGES) {
        return "Unknown";
    }
    return lang_names[lang];
}

int i18n_manager_get_language_count(void)
{
    return MAX_LANGUAGES;
}

language_t i18n_manager_get_language_by_index(int index)
{
    if (index < 0 || index >= MAX_LANGUAGES) {
        return LANG_EN;
    }
    return (language_t)index;
}