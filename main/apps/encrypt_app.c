#include "encrypt_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "ui_animation.h"
#include "input_tool.h"
#include "esp_log.h"
#include "dirent.h"
#include "sys/stat.h"
#include "aes/esp_aes.h"
#include "tinycrypt/sha256.h"
#include "esp_random.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static const char *TAG = "ENCRYPT_APP";

#define MAX_FILES 100
#define MAX_FILENAME 256
#define BUFFER_SIZE 4096
#define AES_KEY_SIZE 32
#define AES_IV_SIZE 16

typedef struct {
    char name[MAX_FILENAME];
    char path[MAX_FILENAME];
    int is_dir;
    size_t size;
} file_info_t;

static file_info_t file_list[MAX_FILES];
static int file_count = 0;
static int selected_file = -1;
static char password[32] = {0};
static char current_dir[MAX_FILENAME] = "/sdcard";
static input_tool_t *input_tool;

static void file_click_cb(lv_event_t *e);

static lv_obj_t *file_list_obj = NULL;
static lv_obj_t *password_input = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *file_count_label = NULL;

static void derive_key(const char *password, unsigned char *key)
{
    unsigned char salt[8] = {0x53, 0x41, 0x4C, 0x54, 0x45, 0x44, 0x5F, 0x4B};
    
    struct tc_sha256_state_struct ctx;
    tc_sha256_init(&ctx);
    tc_sha256_update(&ctx, (const uint8_t *)password, strlen(password));
    tc_sha256_update(&ctx, salt, sizeof(salt));
    tc_sha256_final(key, &ctx);
}

static void generate_iv(unsigned char *iv)
{
    for (int i = 0; i < AES_IV_SIZE; i++) {
        iv[i] = (unsigned char)(esp_random() & 0xFF);
    }
}

static void pkcs7_pad(unsigned char *data, size_t data_len, size_t block_size, size_t *padded_len)
{
    size_t padding_len = block_size - (data_len % block_size);
    *padded_len = data_len + padding_len;
    
    for (size_t i = data_len; i < *padded_len; i++) {
        data[i] = (unsigned char)padding_len;
    }
}

static int pkcs7_unpad(unsigned char *data, size_t *data_len)
{
    if (*data_len < 1) return -1;
    
    unsigned char padding_len = data[*data_len - 1];
    if (padding_len > AES_IV_SIZE) return -1;
    
    for (size_t i = 0; i < padding_len; i++) {
        if (data[*data_len - 1 - i] != padding_len) return -1;
    }
    
    *data_len -= padding_len;
    return 0;
}

static int encrypt_file(const char *input_path, const char *output_path, const char *pass)
{
    FILE *fin = fopen(input_path, "rb");
    if (!fin) return -1;
    
    FILE *fout = fopen(output_path, "wb");
    if (!fout) { fclose(fin); return -1; }
    
    fseek(fin, 0, SEEK_END);
    size_t file_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    
    unsigned char key[AES_KEY_SIZE];
    derive_key(pass, key);
    
    unsigned char iv[AES_IV_SIZE];
    generate_iv(iv);
    
    fwrite(iv, 1, AES_IV_SIZE, fout);
    
    esp_aes_context aes_ctx;
    esp_aes_init(&aes_ctx);
    esp_aes_setkey(&aes_ctx, key, AES_KEY_SIZE * 8);
    
    unsigned char buffer[BUFFER_SIZE + AES_IV_SIZE];
    unsigned char cipher_block[AES_IV_SIZE];
    size_t bytes_read;
    size_t total_read = 0;
    
    memcpy(cipher_block, iv, AES_IV_SIZE);
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fin)) > 0) {
        total_read += bytes_read;
        
        size_t padded_len;
        if (total_read == file_size) {
            pkcs7_pad(buffer, bytes_read, AES_IV_SIZE, &padded_len);
        } else {
            padded_len = bytes_read;
        }
        
        size_t offset = 0;
        while (offset < padded_len) {
            size_t block_size = (padded_len - offset) >= AES_IV_SIZE ? AES_IV_SIZE : (padded_len - offset);
            
            for (size_t i = 0; i < block_size; i++) {
                buffer[offset + i] ^= cipher_block[i];
            }
            
            if (block_size == AES_IV_SIZE) {
                esp_aes_crypt_ecb(&aes_ctx, 1, buffer + offset, cipher_block);
                fwrite(cipher_block, 1, AES_IV_SIZE, fout);
            } else {
                unsigned char temp_block[AES_IV_SIZE] = {0};
                memcpy(temp_block, buffer + offset, block_size);
                
                for (size_t i = 0; i < AES_IV_SIZE; i++) {
                    temp_block[i] ^= cipher_block[i];
                }
                
                esp_aes_crypt_ecb(&aes_ctx, 1, temp_block, cipher_block);
                fwrite(cipher_block, 1, AES_IV_SIZE, fout);
            }
            
            offset += block_size;
        }
    }
    
    esp_aes_free(&aes_ctx);
    fclose(fin);
    fclose(fout);
    return 0;
}

static int decrypt_file(const char *input_path, const char *output_path, const char *pass)
{
    FILE *fin = fopen(input_path, "rb");
    if (!fin) return -1;
    
    FILE *fout = fopen(output_path, "wb");
    if (!fout) { fclose(fin); return -1; }
    
    fseek(fin, 0, SEEK_END);
    size_t file_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    
    if (file_size < AES_IV_SIZE) { fclose(fin); fclose(fout); return -1; }
    
    unsigned char key[AES_KEY_SIZE];
    derive_key(pass, key);
    
    unsigned char iv[AES_IV_SIZE];
    fread(iv, 1, AES_IV_SIZE, fin);
    
    esp_aes_context aes_ctx;
    esp_aes_init(&aes_ctx);
    esp_aes_setkey(&aes_ctx, key, AES_KEY_SIZE * 8);
    
    unsigned char buffer[BUFFER_SIZE];
    unsigned char cipher_block[AES_IV_SIZE];
    unsigned char prev_block[AES_IV_SIZE];
    size_t bytes_read;
    size_t total_read = AES_IV_SIZE;
    
    memcpy(prev_block, iv, AES_IV_SIZE);
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fin)) > 0) {
        total_read += bytes_read;
        
        size_t offset = 0;
        while (offset < bytes_read) {
            size_t block_size = (bytes_read - offset) >= AES_IV_SIZE ? AES_IV_SIZE : (bytes_read - offset);
            
            memcpy(cipher_block, buffer + offset, block_size);
            if (block_size < AES_IV_SIZE) {
                memset(cipher_block + block_size, 0, AES_IV_SIZE - block_size);
            }
            
            unsigned char plain_block[AES_IV_SIZE];
            esp_aes_crypt_ecb(&aes_ctx, 0, cipher_block, plain_block);
            
            for (size_t i = 0; i < AES_IV_SIZE; i++) {
                plain_block[i] ^= prev_block[i];
            }
            
            memcpy(prev_block, cipher_block, AES_IV_SIZE);
            
            size_t write_size = AES_IV_SIZE;
            if (total_read == file_size) {
                write_size = AES_IV_SIZE;
            }
            
            fwrite(plain_block, 1, write_size, fout);
            offset += block_size;
        }
    }
    
    esp_aes_free(&aes_ctx);
    fclose(fin);
    fclose(fout);
    
    fin = fopen(output_path, "rb+");
    if (!fin) return -1;
    
    fseek(fin, 0, SEEK_END);
    size_t decrypted_size = ftell(fin);
    
    if (pkcs7_unpad(buffer, &decrypted_size) != 0) {
        fclose(fin);
        remove(output_path);
        return -1;
    }
    
    ftruncate(fileno(fin), decrypted_size);
    fclose(fin);
    
    return 0;
}

static void load_files(void)
{
    file_count = 0;
    
    DIR *dir = opendir(current_dir);
    if (!dir) return;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && file_count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        snprintf(file_list[file_count].path, sizeof(file_list[file_count].path), "%.*s/%.*s", 
                 (int)(sizeof(file_list[file_count].path)/2 - 1), current_dir, 
                 (int)(sizeof(file_list[file_count].path)/2 - 1), entry->d_name);
        strncpy(file_list[file_count].name, entry->d_name, sizeof(file_list[file_count].name) - 1);
        
        if (entry->d_type == DT_DIR) {
            file_list[file_count].is_dir = 1;
            file_list[file_count].size = 0;
        } else {
            file_list[file_count].is_dir = 0;
            struct stat st;
            if (stat(file_list[file_count].path, &st) == 0) {
                file_list[file_count].size = st.st_size;
            }
        }
        
        file_count++;
    }
    
    closedir(dir);
}

static void update_file_list(void)
{
    if (!file_list_obj) return;
    
    lv_obj_clean(file_list_obj);
    
    for (int i = 0; i < file_count; i++) {
        lv_obj_t *btn = lv_btn_create(file_list_obj);
        lv_obj_set_size(btn, 400, 50);
        lv_obj_set_style_bg_color(btn, i == selected_file ? lv_color_hex(0x4a4a6a) : lv_color_hex(0x252540), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x5a5a8a), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_shadow_width(btn, 4, 0);
        lv_obj_set_style_shadow_color(btn, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(btn, 30, 0);
        
        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
        
        lv_obj_t *icon_label = lv_label_create(btn);
        lv_label_set_text(icon_label, file_list[i].is_dir ? "📁" : "📄");
        lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0x87CEEB), 0);
        lv_obj_align(icon_label, LV_ALIGN_LEFT_MID, 12, 0);
        
        lv_obj_t *label = lv_label_create(btn);
        char label_text[64];
        if (file_list[i].is_dir) {
            snprintf(label_text, sizeof(label_text), "%.*s", (int)(sizeof(label_text) - 2), file_list[i].name);
        } else {
            snprintf(label_text, sizeof(label_text), "%.*s (%d bytes)", (int)(sizeof(label_text) - 14), file_list[i].name, (int)file_list[i].size);
        }
        lv_label_set_text(label, label_text);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xCCCCCC), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 45, 0);
        
        lv_obj_add_event_cb(btn, file_click_cb, LV_EVENT_CLICKED, NULL);
    }
    
    if (file_count_label) {
        char count_str[32];
        snprintf(count_str, sizeof(count_str), "%d files", file_count);
        lv_label_set_text(file_count_label, count_str);
    }
}

static void file_click_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    selected_file = (int)(intptr_t)lv_obj_get_user_data(btn);
    update_file_list();
}

static void open_dir(int index)
{
    if (index < 0 || index >= file_count) return;
    if (!file_list[index].is_dir) return;
    
    strncpy(current_dir, file_list[index].path, sizeof(current_dir) - 1);
    selected_file = -1;
    load_files();
    update_file_list();
    
    if (status_label) {
        lv_label_set_text(status_label, current_dir);
    }
}

static void go_back(void)
{
    char *last_slash = strrchr(current_dir, '/');
    if (last_slash && last_slash != current_dir) {
        *last_slash = '\0';
    } else {
        strcpy(current_dir, "/sdcard");
    }
    
    selected_file = -1;
    load_files();
    update_file_list();
    
    if (status_label) {
        lv_label_set_text(status_label, current_dir);
    }
}

static void back_dir_click_cb(lv_event_t *e)
{
    go_back();
}

static void do_encrypt(lv_event_t *e)
{
    if (selected_file < 0 || selected_file >= file_count) {
        if (status_label) lv_label_set_text(status_label, "Select a file first!");
        return;
    }
    
    if (file_list[selected_file].is_dir) {
        if (status_label) lv_label_set_text(status_label, "Cannot encrypt directory!");
        return;
    }
    
    const char *pwd = lv_textarea_get_text(password_input);
    strncpy(password, pwd ? pwd : "", sizeof(password) - 1);
    if (strlen(password) < 4) {
        if (status_label) lv_label_set_text(status_label, "Password too short!");
        return;
    }
    
    char output_path[MAX_FILENAME + 16];
    snprintf(output_path, sizeof(output_path), "%s.encrypted", file_list[selected_file].path);
    
    if (status_label) lv_label_set_text(status_label, "Encrypting...");
    
    int ret = encrypt_file(file_list[selected_file].path, output_path, password);
    
    if (ret == 0) {
        if (status_label) lv_label_set_text(status_label, "Encryption successful!");
        load_files();
        update_file_list();
    } else {
        if (status_label) lv_label_set_text(status_label, "Encryption failed!");
    }
}

static void do_decrypt(lv_event_t *e)
{
    if (selected_file < 0 || selected_file >= file_count) {
        if (status_label) lv_label_set_text(status_label, "Select a file first!");
        return;
    }
    
    if (file_list[selected_file].is_dir) {
        if (status_label) lv_label_set_text(status_label, "Cannot decrypt directory!");
        return;
    }
    
    const char *name = file_list[selected_file].name;
    int len = strlen(name);
    if (len < 11 || strcmp(name + len - 11, ".encrypted") != 0) {
        if (status_label) lv_label_set_text(status_label, "Not an encrypted file!");
        return;
    }
    
    const char *pwd = lv_textarea_get_text(password_input);
    strncpy(password, pwd ? pwd : "", sizeof(password) - 1);
    if (strlen(password) < 4) {
        if (status_label) lv_label_set_text(status_label, "Password too short!");
        return;
    }
    
    char output_path[MAX_FILENAME];
    strncpy(output_path, file_list[selected_file].path, sizeof(output_path) - 1);
    output_path[strlen(output_path) - 11] = '\0';
    
    if (status_label) lv_label_set_text(status_label, "Decrypting...");
    
    int ret = decrypt_file(file_list[selected_file].path, output_path, password);
    
    if (ret == 0) {
        if (status_label) lv_label_set_text(status_label, "Decryption successful!");
        load_files();
        update_file_list();
    } else {
        if (status_label) lv_label_set_text(status_label, "Decryption failed!");
    }
}

static void back_button_cb(lv_event_t *e)
{
    app_manager_go_home();
}

void encrypt_app_on_exit(void)
{
    if (file_list_obj) {
        lv_obj_clean(file_list_obj);
        file_list_obj = NULL;
    }
    if (status_label) {
        status_label = NULL;
    }
    if (password_input) {
        password_input = NULL;
    }
    if (input_tool) {
        input_tool_destroy(input_tool);
        input_tool = NULL;
    }
    file_count = 0;
    selected_file = 0;
    ESP_LOGI(TAG, "Encrypt app exited and cleaned up");
}

lv_obj_t *encrypt_app_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "File Encryptor");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, current_dir);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x87CEEB), 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 20, 80);

    file_count_label = lv_label_create(scr);
    lv_label_set_text(file_count_label, "0 files");
    lv_obj_set_style_text_font(file_count_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(file_count_label, lv_color_hex(0x4CAF50), 0);
    lv_obj_align(file_count_label, LV_ALIGN_TOP_RIGHT, -20, 80);

    lv_obj_t *file_card = lv_obj_create(scr);
    lv_obj_set_size(file_card, 440, 320);
    lv_obj_set_style_bg_color(file_card, lv_color_hex(0x202038), 0);
    lv_obj_set_style_border_width(file_card, 0, 0);
    lv_obj_set_style_radius(file_card, 16, 0);
    lv_obj_set_style_shadow_width(file_card, 8, 0);
    lv_obj_set_style_shadow_color(file_card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(file_card, 60, 0);
    lv_obj_align(file_card, LV_ALIGN_TOP_MID, 0, 120);

    file_list_obj = lv_obj_create(file_card);
    lv_obj_set_size(file_list_obj, 420, 290);
    lv_obj_align(file_list_obj, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *back_dir_btn = lv_btn_create(scr);
    lv_obj_set_size(back_dir_btn, 440, 48);
    lv_obj_set_style_bg_color(back_dir_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(back_dir_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_dir_btn, 0, 0);
    lv_obj_set_style_radius(back_dir_btn, 10, 0);
    lv_obj_set_style_shadow_width(back_dir_btn, 4, 0);
    lv_obj_set_style_shadow_color(back_dir_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(back_dir_btn, 40, 0);
    lv_obj_align(back_dir_btn, LV_ALIGN_TOP_MID, 0, 450);
    lv_obj_add_event_cb(back_dir_btn, back_dir_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_dir_label = lv_label_create(back_dir_btn);
    lv_label_set_text(back_dir_label, "← Go Back");
    lv_obj_set_style_text_font(back_dir_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_dir_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_dir_label);

    lv_obj_t *pwd_label = lv_label_create(scr);
    lv_label_set_text(pwd_label, "Password:");
    lv_obj_set_style_text_font(pwd_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(pwd_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(pwd_label, LV_ALIGN_TOP_LEFT, 20, 510);

    password_input = lv_textarea_create(scr);
    lv_textarea_set_password_mode(password_input, true);
    lv_textarea_set_max_length(password_input, 32);
    lv_obj_set_size(password_input, 280, 48);
    lv_obj_set_style_bg_color(password_input, lv_color_hex(0x252540), 0);
    lv_obj_set_style_text_font(password_input, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(password_input, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(password_input, 0, 0);
    lv_obj_set_style_radius(password_input, 10, 0);
    lv_obj_align(password_input, LV_ALIGN_TOP_LEFT, 120, 510);

    lv_obj_t *encrypt_btn = lv_btn_create(scr);
    lv_obj_set_size(encrypt_btn, 180, 56);
    lv_obj_set_style_bg_color(encrypt_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_bg_color(encrypt_btn, lv_color_hex(0x388E3C), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(encrypt_btn, 0, 0);
    lv_obj_set_style_radius(encrypt_btn, 12, 0);
    lv_obj_set_style_shadow_width(encrypt_btn, 6, 0);
    lv_obj_set_style_shadow_color(encrypt_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(encrypt_btn, 50, 0);
    lv_obj_align(encrypt_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_event_cb(encrypt_btn, do_encrypt, LV_EVENT_CLICKED, NULL);
    lv_obj_t *encrypt_label = lv_label_create(encrypt_btn);
    lv_label_set_text(encrypt_label, "🔐 Encrypt");
    lv_obj_set_style_text_font(encrypt_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(encrypt_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(encrypt_label);

    lv_obj_t *decrypt_btn = lv_btn_create(scr);
    lv_obj_set_size(decrypt_btn, 180, 56);
    lv_obj_set_style_bg_color(decrypt_btn, lv_color_hex(0xFF9800), 0);
    lv_obj_set_style_bg_color(decrypt_btn, lv_color_hex(0xF57C00), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(decrypt_btn, 0, 0);
    lv_obj_set_style_radius(decrypt_btn, 12, 0);
    lv_obj_set_style_shadow_width(decrypt_btn, 6, 0);
    lv_obj_set_style_shadow_color(decrypt_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(decrypt_btn, 50, 0);
    lv_obj_align(decrypt_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_add_event_cb(decrypt_btn, do_decrypt, LV_EVENT_CLICKED, NULL);
    lv_obj_t *decrypt_label = lv_label_create(decrypt_btn);
    lv_label_set_text(decrypt_label, "🔓 Decrypt");
    lv_obj_set_style_text_font(decrypt_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(decrypt_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(decrypt_label);

    input_tool = input_tool_create(scr);
    input_tool_attach_textarea(input_tool, password_input);
    
    load_files();
    update_file_list();

    ui_animation_slide(scr, LV_DIR_RIGHT, 300);

    ESP_LOGI(TAG, "Encrypt app created with modern design");
    return scr;
}
