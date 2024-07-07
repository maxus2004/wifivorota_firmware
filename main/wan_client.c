#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "wan_client.h"
#include "query_decoder.h"
#include "api.h"
#include "config.h"
#include <esp_camera.h>
#include <esp_websocket_client.h>

static const char* TAG = "WiFIVorota-WanClient";

static esp_websocket_client_handle_t controlSocket;

bool wanStream_started = false;

static char apiURI[256];
static char streamURI[256];
static char query_str[1024];
char response[1024];

static void controlSocket_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        break;
    case WEBSOCKET_EVENT_DATA:
        if (data->op_code != 1) break;
        if (data->data_len > 1023) {
            break;
        }

        memcpy(query_str, data->data_ptr, data->data_len);
        query_str[data->data_len] = '\0';

        char* query_start = strchr(query_str, '?');
        if (query_start == NULL) {
            break;
        }
        query_t query = queryDecode(query_start);
        api_process(query, response, true);
        esp_websocket_client_send_text(controlSocket, response, strlen(response), portMAX_DELAY);

        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
        break;
    }
}

void wanClient_restart(){
    esp_websocket_client_close(controlSocket,portMAX_DELAY);
    sprintf(apiURI, "wss://vorota.servermaksa.ru/api/?login=%s&version=%s",config_getUserLogin(),config_getVersion());
    esp_websocket_client_config_t controlSocket_cfg = {
        .uri = apiURI,
    };
    controlSocket = esp_websocket_client_init(&controlSocket_cfg);
    esp_websocket_register_events(controlSocket, WEBSOCKET_EVENT_ANY, controlSocket_event_handler, (void*)controlSocket);
    esp_websocket_client_start(controlSocket);
}

void wanClient_start() {
    sprintf(apiURI, "wss://vorota.servermaksa.ru/api/?login=%s&version=%s",config_getUserLogin(),config_getVersion());
    esp_websocket_client_config_t controlSocket_cfg = {
        .uri = apiURI,
    };
    controlSocket = esp_websocket_client_init(&controlSocket_cfg);
    esp_websocket_register_events(controlSocket, WEBSOCKET_EVENT_ANY, controlSocket_event_handler, (void*)controlSocket);
    esp_websocket_client_start(controlSocket);
}

void wanClient_stream_task(void* pvParameters) {
    sprintf(streamURI, "wss://vorota.servermaksa.ru/stream/?login=%s&version=%s",config_getUserLogin(),config_getVersion());
    esp_websocket_client_config_t streamSocket_cfg = {
        .uri = streamURI,
        .disable_auto_reconnect = true
    };
    esp_websocket_client_handle_t streamSocket = esp_websocket_client_init(&streamSocket_cfg);
    esp_websocket_client_start(streamSocket);

    ESP_LOGI(TAG, "WAN stream starting...");

    TickType_t connectStart = xTaskGetTickCount();
    while(1){
        vTaskDelay(pdMS_TO_TICKS(100));
        if(esp_websocket_client_is_connected(streamSocket))break;
        if(xTaskGetTickCount()-connectStart > pdMS_TO_TICKS(10000))break;
    }

    ESP_LOGI(TAG, "WAN stream started");

    camera_fb_t* fb = 0;
    while (esp_websocket_client_is_connected(streamSocket)) {
        fb = esp_camera_fb_get();
        if(fb){
            int res = esp_websocket_client_send_bin(streamSocket, (const char *)(fb->buf), fb->len, portMAX_DELAY);
            esp_camera_fb_return(fb);
            if (res != fb->len)break;
        }
    }

    esp_websocket_client_stop(streamSocket);
    esp_websocket_client_destroy(streamSocket);

    ESP_LOGI(TAG, "WAN stream stopped");
    wanStream_started = false;
    vTaskDelete(NULL);
}

void wanClient_startStream() {
    if(wanStream_started)return;
    wanStream_started = true;
    xTaskCreate(wanClient_stream_task, "wan_stream", 8192, NULL, 5, NULL);
}