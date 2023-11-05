#include "config.h"
#include <string.h>
#include <stdbool.h>
#include <esp_log.h>
#include <nvs.h>
#include "wan_client.h"

static const char* TAG = "WiFIVorota-Config";

char version[] = "1.0.3";
char wifi_ssid[32] = "wifi";
char wifi_password[64] = "password";
char user_login[32] = "login";
char user_password[64] = "password";
char hostname[] = "wifivorota";
char button_names[4][32] = { "Открыть/Закрыть","","","" };
uint8_t btn_invert = 0b1111;

void config_setWifiSsid(char* value) {
    strcpy(wifi_ssid, value);
}
void config_setWifiPassword(char* value) {
    strcpy(wifi_password, value);
}
void config_setUserLogin(char* value) {
    strcpy(user_login, value);
    wanClient_restart();
}
void config_setUserPassword(char* value) {
    strcpy(user_password, value);
}
void config_setButton(int id, char* value) {
    strcpy(button_names[id], value);
}

char* config_getWifiSsid() {
    return wifi_ssid;
}
char* config_getWifiPassword() {
    return wifi_password;
}
char* config_getUserLogin() {
    return user_login;
}
char* config_getUserPassword() {
    return user_password;
}
char* config_getHostname() {
    return hostname;
}
char* config_getButton(int id) {
    return button_names[id];
}
char* config_getVersion() {
    return version;
}
bool config_getBtnInvert(int id) {
    return (btn_invert >> id) & 1;
}

void config_save() {
    ESP_LOGI(TAG, "saving config...");

    nvs_handle_t handle;
    nvs_open("storage", NVS_READWRITE, &handle);
    nvs_set_blob(handle, "wifi_ssid", wifi_ssid, 32);
    nvs_set_blob(handle, "wifi_password", wifi_password, 64);
    nvs_set_blob(handle, "user_login", user_login, 32);
    nvs_set_blob(handle, "user_password", user_password, 64);
    nvs_set_blob(handle, "button_names", button_names, 32*4);
    nvs_set_u8(handle, "btnInvert", btn_invert);
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "saved config");
    ESP_LOGI(TAG, "wifi_ssid=%s", wifi_ssid);
    ESP_LOGI(TAG, "wifi_password=%s", wifi_password);
    ESP_LOGI(TAG, "user_login=%s", user_login);
    ESP_LOGI(TAG, "user_password=%s", user_password);
    ESP_LOGI(TAG, "hostname=%s", hostname);
    ESP_LOGI(TAG, "button_names={\"%s\", \"%s\", \"%s\", \"%s\"}", button_names[0], button_names[1], button_names[2], button_names[3]);
    ESP_LOGI(TAG, "btn_invert={%i, %i, %i, %i}", (btn_invert >> 0) & 1, (btn_invert >> 1) & 1, (btn_invert >> 2) & 1, (btn_invert >> 3) & 1);
}
void config_load() {
    ESP_LOGI(TAG, "loading config...");

    nvs_handle_t handle;
    nvs_open("storage", NVS_READONLY, &handle);
    size_t len = 32;
    nvs_get_blob(handle, "wifi_ssid", wifi_ssid, &len);
    len = 64;
    nvs_get_blob(handle, "wifi_password", wifi_password, &len);
    len = 32;
    nvs_get_blob(handle, "user_login", user_login, &len);
    len = 64;
    nvs_get_blob(handle, "user_password", user_password, &len);
    len = 4*32;
    nvs_get_blob(handle, "button_names", button_names, &len);
    nvs_get_u8(handle, "btnInvert", &btn_invert);
    nvs_close(handle);

    ESP_LOGI(TAG, "loaded config");
    ESP_LOGI(TAG, "wifi_ssid=%s", wifi_ssid);
    ESP_LOGI(TAG, "wifi_password=%s", wifi_password);
    ESP_LOGI(TAG, "user_login=%s", user_login);
    ESP_LOGI(TAG, "user_password=%s", user_password);
    ESP_LOGI(TAG, "hostname=%s", hostname);
    ESP_LOGI(TAG, "button_names={\"%s\", \"%s\", \"%s\", \"%s\"}", button_names[0], button_names[1], button_names[2], button_names[3]);
    ESP_LOGI(TAG, "btn_invert={%i, %i, %i, %i}", (btn_invert >> 0) & 1, (btn_invert >> 1) & 1, (btn_invert >> 2) & 1, (btn_invert >> 3) & 1);
}