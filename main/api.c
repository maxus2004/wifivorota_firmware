#include "api.h"
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "camera.h"
#include "buttons.h"
#include "wifi.h"
#include <esp_log.h>
#include <wan_client.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_ota_ops.h>

static const char* TAG = "WiFIVorota-API";

static void reboot_task(void* pvParameters) {
    ESP_LOGI(TAG, "restarting in 2 seconds...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
}

void api_process(query_t query, char* response) {
    ESP_LOGI(TAG, "api cmd: %s", queryStr(query, "cmd"));
    char* login = queryStr(query, "login");
    char* password = queryStr(query, "password");
    if (strcmp(login, config_getUserLogin()) != 0 || strcmp(password, config_getUserPassword()) != 0) {
        sprintf(response, "result=UNAUTHORIZED");
        ESP_LOGI(TAG, "unauthorized");
        return;
    }

    char* cmd = queryStr(query, "cmd");

    if (strcmp(cmd, "info") == 0) {
        sprintf(response, "result=OK&version=%s&login=%s&rssi=%i&led=%i&free_ram_internal=%i&free_ram_spi=%i&", config_getVersion(), config_getUserLogin(), wifi_getRSSI(), led_status(), heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    } else if (strcmp(cmd, "press_btn") == 0) {
        int btn_id = queryInt(query, "id");
        buttons_press(btn_id);
        sprintf(response, "result=OK");
    } else if (strcmp(cmd, "camera_config") == 0) {
        if (queryContains(query, "xclk")) {
            camera_setXclk(queryInt(query, "xclk"));
        }
        if (queryContains(query, "framesize")) {
            camera_setFramesize(queryInt(query, "framesize"));
        }
        if (queryContains(query, "quality")) {
            camera_setQuality(queryInt(query, "quality"));
        }
        sprintf(response, "result=OK&xclk=%i&framesize=%i&quality=%i", camera_getXclk(), camera_getFramesize(), camera_getQuality());
    } else if (strcmp(cmd, "config") == 0) {
        if (queryContains(query, "wifi_ssid")) {
            config_setWifiSsid(queryStr(query, "wifi_ssid"));
        }
        if (queryContains(query, "wifi_password")) {
            config_setWifiPassword(queryStr(query, "wifi_password"));
        }
        if (queryContains(query, "user_login")) {
            config_setUserLogin(queryStr(query, "user_login"));
        }
        if (queryContains(query, "user_password")) {
            config_setUserPassword(queryStr(query, "user_password"));
        }
        if (queryContains(query, "btn0_name")) {
            config_setButton(0,queryStr(query, "btn0_name"));
        }
        if (queryContains(query, "btn1_name")) {
            config_setButton(1,queryStr(query, "btn1_name"));
        }
        if (queryContains(query, "btn2_name")) {
            config_setButton(2,queryStr(query, "btn2_name"));
        }
        if (queryContains(query, "btn3_name")) {
            config_setButton(3,queryStr(query, "btn3_name"));
        }
        sprintf(response, "result=OK&wifi_ssid=%s&wifi_password=%s&user_login=%s&user_password=%s&btn0_name=%s&btn1_name=%s&btn2_name=%s&btn3_name=%s",
            config_getWifiSsid(), config_getWifiPassword(), config_getUserLogin(), config_getUserPassword(), config_getButton(0), config_getButton(1), config_getButton(2), config_getButton(3));
    } else if (strcmp(cmd, "config_save") == 0) {
        config_save();
        sprintf(response, "result=OK&wifi_ssid=%s&wifi_password=%s&user_login=%s&user_password=%s&btn0_name=%s&btn1_name=%s&btn2_name=%s&btn3_name=%s",
            config_getWifiSsid(), config_getWifiPassword(), config_getUserLogin(), config_getUserPassword(), config_getButton(0), config_getButton(1), config_getButton(2), config_getButton(3));
    } else if (strcmp(cmd, "wan_stream") == 0) {
        wanClient_startStream();
        sprintf(response, "result=OK");
    } else if (strcmp(cmd, "update") == 0) {
        char* url = queryStr(query, "url");
        esp_http_client_config_t config = {
        .url = url,
        };
        esp_https_ota_config_t ota_config = {
            .http_config = &config,
        };
        esp_err_t ret = esp_https_ota(&ota_config);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "uploaded new firmware");
            sprintf(response, "result=OK");
        } else {
            ESP_LOGI(TAG, "failed to update firmware");
            sprintf(response, "result=FAILED");
        }
    } else if (strcmp(cmd, "reboot") == 0) {
        sprintf(response, "result=OK");
        xTaskCreate(reboot_task, "reboot_task", 2048, NULL, 5, NULL);
    } else if (strcmp(cmd, "update_validate") == 0) {
        esp_ota_mark_app_valid_cancel_rollback();
        sprintf(response, "result=OK");
    } else {
        ESP_LOGI(TAG, "unknown api command: %s", cmd);
        sprintf(response, "result=UNKNOWN_COMMAND");
    }
}