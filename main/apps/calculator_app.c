#include "calculator_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "ui_utils.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static const char *TAG = "CALCULATOR";

static lv_obj_t *display = NULL;
static lv_obj_t *scr = NULL;

static char current_input[128] = "";
static char previous_input[128] = "";
static char operation = '\0';
static bool reset_display = false;

static void back_button_cb(lv_event_t *e);
static void handle_button(lv_event_t *e);

static void update_display(void)
{
    if (!display) return;
    lv_label_set_text(display, current_input);
}

static void handle_number(char num)
{
    if (reset_display) {
        strcpy(current_input, "");
        reset_display = false;
    }
    
    if (strlen(current_input) < 16) {
        char buf[2] = {num, '\0'};
        strcat(current_input, buf);
        update_display();
    }
}

static void handle_operation(char op)
{
    if (strlen(current_input) == 0) return;
    
    strcpy(previous_input, current_input);
    operation = op;
    reset_display = true;
    update_display();
}

static void handle_clear(void)
{
    strcpy(current_input, "");
    strcpy(previous_input, "");
    operation = '\0';
    reset_display = false;
    update_display();
}

static void handle_delete(void)
{
    if (strlen(current_input) > 0) {
        current_input[strlen(current_input) - 1] = '\0';
        update_display();
    }
}

static void handle_decimal(void)
{
    if (reset_display) {
        strcpy(current_input, "0.");
        reset_display = false;
    } else if (strchr(current_input, '.') == NULL) {
        strcat(current_input, ".");
    }
    update_display();
}

static void handle_equals(void)
{
    if (operation == '\0' || strlen(previous_input) == 0) return;
    
    double prev = atof(previous_input);
    double curr = atof(current_input);
    double result = 0;
    
    switch (operation) {
        case '+': result = prev + curr; break;
        case '-': result = prev - curr; break;
        case '*': result = prev * curr; break;
        case '/': 
            if (curr != 0) result = prev / curr;
            else {
                strcpy(current_input, "Error");
                operation = '\0';
                update_display();
                return;
            }
            break;
        case '%': result = fmod(prev, curr); break;
        case '^': result = pow(prev, curr); break;
    }
    
    char result_str[64];
    snprintf(result_str, sizeof(result_str), "%.10g", result);
    strcpy(current_input, result_str);
    operation = '\0';
    reset_display = true;
    update_display();
}

static void handle_button(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    const char *text = lv_label_get_text(lv_obj_get_child(btn, 0));
    
    if (!text) return;
    
    if (strlen(text) == 1) {
        char c = text[0];
        if (c >= '0' && c <= '9') {
            handle_number(c);
        } else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%') {
            handle_operation(c);
        } else if (c == '=') {
            handle_equals();
        } else if (c == '.') {
            handle_decimal();
        }
    } else if (strcmp(text, "C") == 0) {
        handle_clear();
    } else if (strcmp(text, "DEL") == 0) {
        handle_delete();
    } else if (strcmp(text, "x²") == 0) {
        double val = atof(current_input);
        char result_str[64];
        snprintf(result_str, sizeof(result_str), "%.10g", val * val);
        strcpy(current_input, result_str);
        update_display();
    } else if (strcmp(text, "√") == 0) {
        double val = atof(current_input);
        if (val >= 0) {
            char result_str[64];
            snprintf(result_str, sizeof(result_str), "%.10g", sqrt(val));
            strcpy(current_input, result_str);
        } else {
            strcpy(current_input, "Error");
        }
        update_display();
    } else if (strcmp(text, "1/x") == 0) {
        double val = atof(current_input);
        if (val != 0) {
            char result_str[64];
            snprintf(result_str, sizeof(result_str), "%.10g", 1.0 / val);
            strcpy(current_input, result_str);
        } else {
            strcpy(current_input, "Error");
        }
        update_display();
    } else if (strcmp(text, "^") == 0) {
        handle_operation('^');
    } else if (strcmp(text, "+/-") == 0) {
        if (current_input[0] == '-') {
            memmove(current_input, current_input + 1, strlen(current_input));
        } else {
            char temp[129];
            snprintf(temp, sizeof(temp), "-%s", current_input);
            strcpy(current_input, temp);
        }
        update_display();
    }
}

static lv_obj_t *create_calc_button(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y, 
                                    lv_coord_t w, lv_coord_t h, lv_color_t normal_color, lv_color_t pressed_color)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, normal_color, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, pressed_color, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(btn, 40, LV_PART_MAIN);
    lv_obj_set_pos(btn, x, y);
    lv_obj_add_event_cb(btn, handle_button, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(label);
    
    return btn;
}

lv_obj_t *calculator_app_create(void)
{
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    
    status_bar_create(scr);
    
    ui_utils_create_title(scr, "Calculator");
    
    lv_obj_t *back_style_btn = lv_btn_create(scr);
    lv_obj_set_size(back_style_btn, 40, 40);
    lv_obj_set_style_bg_color(back_style_btn, lv_color_hex(0x3a3a5a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(back_style_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_style_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(back_style_btn, 8, LV_PART_MAIN);
    lv_obj_align(back_style_btn, LV_ALIGN_TOP_LEFT, 10, 50);
    lv_obj_add_event_cb(back_style_btn, back_button_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_style_btn);
    lv_label_set_text(back_label, "←");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(back_label);
    
    lv_obj_t *display_frame = lv_obj_create(scr);
    lv_obj_set_size(display_frame, 440, 100);
    lv_obj_set_style_bg_color(display_frame, lv_color_hex(0x252540), LV_PART_MAIN);
    lv_obj_set_style_border_width(display_frame, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(display_frame, 16, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(display_frame, 6, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(display_frame, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(display_frame, 60, LV_PART_MAIN);
    lv_obj_align(display_frame, LV_ALIGN_TOP_MID, 0, 110);
    
    display = lv_label_create(display_frame);
    lv_label_set_text(display, "0");
    lv_obj_set_style_text_font(display, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(display, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_color(display, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(display, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(display, 0, LV_PART_MAIN);
    lv_obj_set_size(display, 400, 60);
    lv_obj_align(display, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_label_set_long_mode(display, LV_LABEL_LONG_SCROLL_CIRCULAR);
    
    handle_clear();
    
    int btn_w = 95;
    int btn_h = 60;
    int padding = 10;
    int start_x = 20;
    int start_y = 230;
    
    create_calc_button(scr, "C",     start_x, start_y, btn_w, btn_h, lv_color_hex(0xE53935), lv_color_hex(0xC62828));
    create_calc_button(scr, "DEL",   start_x + btn_w + padding, start_y, btn_w, btn_h, lv_color_hex(0xE53935), lv_color_hex(0xC62828));
    create_calc_button(scr, "%",     start_x + 2*(btn_w + padding), start_y, btn_w, btn_h, lv_color_hex(0xFF9800), lv_color_hex(0xF57C00));
    create_calc_button(scr, "/",     start_x + 3*(btn_w + padding), start_y, btn_w, btn_h, lv_color_hex(0xFF9800), lv_color_hex(0xF57C00));
    
    start_y += btn_h + padding;
    
    create_calc_button(scr, "7",     start_x, start_y, btn_w, btn_h, lv_color_hex(0x3a3a5a), lv_color_hex(0x4a4a6a));
    create_calc_button(scr, "8",     start_x + btn_w + padding, start_y, btn_w, btn_h, lv_color_hex(0x3a3a5a), lv_color_hex(0x4a4a6a));
    create_calc_button(scr, "9",     start_x + 2*(btn_w + padding), start_y, btn_w, btn_h, lv_color_hex(0x3a3a5a), lv_color_hex(0x4a4a6a));
    create_calc_button(scr, "*",     start_x + 3*(btn_w + padding), start_y, btn_w, btn_h, lv_color_hex(0xFF9800), lv_color_hex(0xF57C00));
    
    start_y += btn_h + padding;
    
    create_calc_button(scr, "4",     start_x, start_y, btn_w, btn_h, lv_color_hex(0x3a3a5a), lv_color_hex(0x4a4a6a));
    create_calc_button(scr, "5",     start_x + btn_w + padding, start_y, btn_w, btn_h, lv_color_hex(0x3a3a5a), lv_color_hex(0x4a4a6a));
    create_calc_button(scr, "6",     start_x + 2*(btn_w + padding), start_y, btn_w, btn_h, lv_color_hex(0x3a3a5a), lv_color_hex(0x4a4a6a));
    create_calc_button(scr, "-",     start_x + 3*(btn_w + padding), start_y, btn_w, btn_h, lv_color_hex(0xFF9800), lv_color_hex(0xF57C00));
    
    start_y += btn_h + padding;
    
    create_calc_button(scr, "1",     start_x, start_y, btn_w, btn_h, lv_color_hex(0x3a3a5a), lv_color_hex(0x4a4a6a));
    create_calc_button(scr, "2",     start_x + btn_w + padding, start_y, btn_w, btn_h, lv_color_hex(0x3a3a5a), lv_color_hex(0x4a4a6a));
    create_calc_button(scr, "3",     start_x + 2*(btn_w + padding), start_y, btn_w, btn_h, lv_color_hex(0x3a3a5a), lv_color_hex(0x4a4a6a));
    create_calc_button(scr, "+",     start_x + 3*(btn_w + padding), start_y, btn_w, btn_h, lv_color_hex(0xFF9800), lv_color_hex(0xF57C00));
    
    start_y += btn_h + padding;
    
    create_calc_button(scr, "+/-",   start_x, start_y, btn_w, btn_h, lv_color_hex(0x3a3a5a), lv_color_hex(0x4a4a6a));
    create_calc_button(scr, "0",     start_x + btn_w + padding, start_y, btn_w, btn_h, lv_color_hex(0x3a3a5a), lv_color_hex(0x4a4a6a));
    create_calc_button(scr, ".",     start_x + 2*(btn_w + padding), start_y, btn_w, btn_h, lv_color_hex(0x3a3a5a), lv_color_hex(0x4a4a6a));
    create_calc_button(scr, "=",     start_x + 3*(btn_w + padding), start_y, btn_w, btn_h, lv_color_hex(0x4CAF50), lv_color_hex(0x388E3C));
    
    start_y += btn_h + padding;
    
    create_calc_button(scr, "x²",    start_x, start_y, btn_w, btn_h, lv_color_hex(0x5a5a7a), lv_color_hex(0x6a6a8a));
    create_calc_button(scr, "√",     start_x + btn_w + padding, start_y, btn_w, btn_h, lv_color_hex(0x5a5a7a), lv_color_hex(0x6a6a8a));
    create_calc_button(scr, "1/x",   start_x + 2*(btn_w + padding), start_y, btn_w, btn_h, lv_color_hex(0x5a5a7a), lv_color_hex(0x6a6a8a));
    create_calc_button(scr, "^",     start_x + 3*(btn_w + padding), start_y, btn_w, btn_h, lv_color_hex(0xFF9800), lv_color_hex(0xF57C00));
    
    ESP_LOGI(TAG, "Calculator app created with modern design");
    
    return scr;
}

static void back_button_cb(lv_event_t *e)
{
    (void)e;
    app_manager_go_home();
}