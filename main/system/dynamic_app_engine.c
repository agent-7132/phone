#include "dynamic_app_engine.h"
#include "app_manager.h"
#include "status_bar.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "DYNAMIC_ENGINE";

typedef struct {
    const char *id;
    lv_obj_t *obj;
} widget_map_t;

static widget_map_t widget_map[32];
static int widget_count = 0;

static void skip_whitespace(const char **json)
{
    while (**json == ' ' || **json == '\t' || **json == '\n' || **json == '\r') {
        (*json)++;
    }
}

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

static int parse_int(const char **json)
{
    skip_whitespace(json);
    int val = 0;
    int sign = 1;
    if (**json == '-') { sign = -1; (*json)++; }
    while (**json >= '0' && **json <= '9') {
        val = val * 10 + (**json - '0');
        (*json)++;
    }
    return val * sign;
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

static uint32_t parse_color(const char **json)
{
    char color_str[16];
    parse_string(json, color_str, sizeof(color_str));
    
    if (color_str[0] == '#') {
        return strtoul(color_str + 1, NULL, 16);
    }
    return 0x1a1a2e;
}

static lv_obj_t *find_widget(const char *id)
{
    for (int i = 0; i < widget_count; i++) {
        if (strcmp(widget_map[i].id, id) == 0) {
            return widget_map[i].obj;
        }
    }
    return NULL;
}

static void add_widget(const char *id, lv_obj_t *obj)
{
    if (widget_count < 32) {
        widget_map[widget_count].id = strdup(id);
        widget_map[widget_count].obj = obj;
        widget_count++;
    }
}

static void clear_widget_map(void)
{
    for (int i = 0; i < widget_count; i++) {
        free((void *)widget_map[i].id);
    }
    widget_count = 0;
}

static lv_align_t parse_align(const char *align_str)
{
    if (strcmp(align_str, "top_mid") == 0) return LV_ALIGN_TOP_MID;
    if (strcmp(align_str, "top_left") == 0) return LV_ALIGN_TOP_LEFT;
    if (strcmp(align_str, "top_right") == 0) return LV_ALIGN_TOP_RIGHT;
    if (strcmp(align_str, "center") == 0) return LV_ALIGN_CENTER;
    if (strcmp(align_str, "left_mid") == 0) return LV_ALIGN_LEFT_MID;
    if (strcmp(align_str, "right_mid") == 0) return LV_ALIGN_RIGHT_MID;
    if (strcmp(align_str, "bottom_mid") == 0) return LV_ALIGN_BOTTOM_MID;
    if (strcmp(align_str, "bottom_left") == 0) return LV_ALIGN_BOTTOM_LEFT;
    if (strcmp(align_str, "bottom_right") == 0) return LV_ALIGN_BOTTOM_RIGHT;
    return LV_ALIGN_CENTER;
}

static void handle_command(const char *command_name, const char *json)
{
    ESP_LOGI(TAG, "Handling command: %s", command_name);
    
    const char *p = json;
    skip_whitespace(&p);
    if (*p != '{') return;
    p++;
    
    char action[32] = {0};
    char target[32] = {0};
    char value[128] = {0};
    
    while (*p != '}' && *p != '\0') {
        skip_whitespace(&p);
        if (*p == ',') { p++; continue; }
        
        char key[32] = {0};
        parse_key(&p, key, sizeof(key));
        
        if (strcmp(key, "action") == 0) {
            parse_string(&p, action, sizeof(action));
        } else if (strcmp(key, "target") == 0) {
            parse_string(&p, target, sizeof(target));
        } else if (strcmp(key, "value") == 0) {
            parse_string(&p, value, sizeof(value));
        } else {
            while (*p != ',' && *p != '}' && *p != '\0') p++;
        }
    }
    
    if (strcmp(action, "set_text") == 0) {
        lv_obj_t *obj = find_widget(target);
        if (obj) {
            lv_label_set_text(obj, value);
        }
    } else if (strcmp(action, "go_home") == 0) {
        app_manager_go_home();
    }
}

static void button_click_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    const char *command_name = (const char *)lv_obj_get_user_data(btn);
    
    if (command_name) {
        const char *commands_json = (const char *)lv_obj_get_user_data(lv_obj_get_parent(btn));
        if (commands_json) {
            const char *p = commands_json;
            skip_whitespace(&p);
            if (*p != '{') return;
            p++;
            
            while (*p != '}' && *p != '\0') {
                skip_whitespace(&p);
                if (*p == ',') { p++; continue; }
                
                char key[32] = {0};
                parse_key(&p, key, sizeof(key));
                
                if (strcmp(key, command_name) == 0) {
                    handle_command(command_name, p);
                    break;
                }
                
                while (*p != ',' && *p != '}' && *p != '\0') p++;
            }
        }
    }
}

static void parse_widget(const char **json, lv_obj_t *parent)
{
    skip_whitespace(json);
    if (**json != '{') return;
    (*json)++;
    
    char type[32] = {0};
    char id[32] = {0};
    char text[128] = {0};
    char font[32] = "montserrat_14";
    char align_str[32] = "center";
    uint32_t color = 0xFFFFFF;
    uint32_t bg_color = 0x2d2d44;
    int width = 100, height = 50;
    int x = 0, y = 0;
    char on_click[32] = {0};
    
    while (**json != '}' && **json != '\0') {
        skip_whitespace(json);
        if (**json == ',') { (*json)++; continue; }
        
        char key[32] = {0};
        parse_key(json, key, sizeof(key));
        
        if (strcmp(key, "type") == 0) {
            parse_string(json, type, sizeof(type));
        } else if (strcmp(key, "id") == 0) {
            parse_string(json, id, sizeof(id));
        } else if (strcmp(key, "text") == 0) {
            parse_string(json, text, sizeof(text));
        } else if (strcmp(key, "font") == 0) {
            parse_string(json, font, sizeof(font));
        } else if (strcmp(key, "color") == 0) {
            color = parse_color(json);
        } else if (strcmp(key, "bg_color") == 0) {
            bg_color = parse_color(json);
        } else if (strcmp(key, "width") == 0) {
            width = parse_int(json);
        } else if (strcmp(key, "height") == 0) {
            height = parse_int(json);
        } else if (strcmp(key, "x") == 0) {
            x = parse_int(json);
        } else if (strcmp(key, "y") == 0) {
            y = parse_int(json);
        } else if (strcmp(key, "align") == 0) {
            parse_string(json, align_str, sizeof(align_str));
        } else if (strcmp(key, "on_click") == 0) {
            parse_string(json, on_click, sizeof(on_click));
        } else {
            while (**json != ',' && **json != '}' && **json != '\0') (*json)++;
        }
    }
    
    (*json)++;
    
    lv_obj_t *obj = NULL;
    if (strcmp(type, "label") == 0) {
        obj = lv_label_create(parent);
        lv_label_set_text(obj, text);
        lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    } else if (strcmp(type, "button") == 0) {
        obj = lv_btn_create(parent);
        lv_obj_set_size(obj, width, height);
        lv_obj_set_style_bg_color(obj, lv_color_hex(bg_color), 0);
        
        lv_obj_t *label = lv_label_create(obj);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
        lv_obj_center(label);
        
        if (on_click[0] != '\0') {
            lv_obj_set_user_data(obj, (void *)strdup(on_click));
            lv_obj_add_event_cb(obj, button_click_cb, LV_EVENT_CLICKED, NULL);
        }
    } else if (strcmp(type, "rect") == 0) {
        obj = lv_obj_create(parent);
        lv_obj_set_size(obj, width, height);
        lv_obj_set_style_bg_color(obj, lv_color_hex(bg_color), 0);
        lv_obj_set_style_border_width(obj, 0, 0);
    }
    
    if (obj) {
        lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, 0);
        
        lv_obj_align(obj, parse_align(align_str), x, y);
        
        if (id[0] != '\0') {
            add_widget(id, obj);
        }
    }
}

static void parse_children(const char **json, lv_obj_t *parent)
{
    skip_whitespace(json);
    if (**json != '[') return;
    (*json)++;
    
    while (**json != ']' && **json != '\0') {
        skip_whitespace(json);
        if (**json == ',') { (*json)++; continue; }
        
        parse_widget(json, parent);
    }
    
    (*json)++;
}

static void parse_screen(const char **json, lv_obj_t *scr, char **commands_json)
{
    skip_whitespace(json);
    if (**json != '{') return;
    (*json)++;
    
    while (**json != '}' && **json != '\0') {
        skip_whitespace(json);
        if (**json == ',') { (*json)++; continue; }
        
        char key[32] = {0};
        parse_key(json, key, sizeof(key));
        
        if (strcmp(key, "background_color") == 0) {
            uint32_t bg_color = parse_color(json);
            lv_obj_set_style_bg_color(scr, lv_color_hex(bg_color), 0);
        } else if (strcmp(key, "children") == 0) {
            parse_children(json, scr);
        } else if (strcmp(key, "commands") == 0) {
            const char *start = *json;
            skip_whitespace(&start);
            if (*start == '{') {
                const char *end = start;
                int brace_count = 1;
                while (brace_count > 0 && *end != '\0') {
                    end++;
                    if (*end == '{') brace_count++;
                    if (*end == '}') brace_count--;
                }
                int len = end - start + 1;
                *commands_json = malloc(len + 1);
                strncpy(*commands_json, start, len);
                (*commands_json)[len] = '\0';
                *json = end + 1;
            }
        } else {
            while (**json != ',' && **json != '}' && **json != '\0') (*json)++;
        }
    }
    
    (*json)++;
}

lv_obj_t *dynamic_app_create_screen(const app_info_t *app)
{
    ESP_LOGI(TAG, "Creating dynamic app screen: %s", app->name);
    
    clear_widget_map();
    
    char layout_path[256];
    snprintf(layout_path, sizeof(layout_path), "%s/%s", app->path, app->data.dynamic.entry_point);
    
    FILE *f = fopen(layout_path, "r");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open layout file: %s", layout_path);
        
        lv_obj_t *scr = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
        lv_obj_set_style_border_width(scr, 0, 0);
        
        status_bar_create(scr);
        
        lv_obj_t *error_label = lv_label_create(scr);
        lv_label_set_text(error_label, "Layout not found!");
        lv_obj_set_style_text_font(error_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(error_label, lv_color_hex(0xFF0000), 0);
        lv_obj_center(error_label);
        
        return scr;
    }
    
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buffer = malloc(len + 1);
    if (!buffer) {
        fclose(f);
        ESP_LOGE(TAG, "Memory allocation failed");
        return NULL;
    }
    
    fread(buffer, 1, len, f);
    buffer[len] = '\0';
    fclose(f);
    
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    
    status_bar_create(scr);
    
    char *commands_json = NULL;
    const char *p = buffer;
    skip_whitespace(&p);
    
    while (*p != '\0') {
        skip_whitespace(&p);
        if (*p == ',') { p++; continue; }
        
        char key[32] = {0};
        parse_key(&p, key, sizeof(key));
        
        if (strcmp(key, "screen") == 0) {
            parse_screen(&p, scr, &commands_json);
        } else {
            while (*p != ',' && *p != '}' && *p != '\0') p++;
        }
    }
    
    if (commands_json) {
        lv_obj_set_user_data(scr, commands_json);
    }
    
    free(buffer);
    
    ESP_LOGI(TAG, "Dynamic app screen created: %s", app->name);
    return scr;
}
