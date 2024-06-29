#include "buttons.h"
#include <freertos/FreeRTOS.h>
#include <driver/gpio.h>
#include <freertos/task.h>
#include <esp_rom_gpio.h>
#include <esp_log.h>
#include "config.h"

static const char* TAG = "WiFIVorota-Buttons";

void buttons_init() {
    esp_rom_gpio_pad_select_gpio(4);
    esp_rom_gpio_pad_select_gpio(13);
    esp_rom_gpio_pad_select_gpio(14);
    esp_rom_gpio_pad_select_gpio(15);
    gpio_set_level(4, !config_getBtnInvert(0));
    gpio_set_level(13, !config_getBtnInvert(1));
    gpio_set_level(14, !config_getBtnInvert(2));
    gpio_set_level(15, !config_getBtnInvert(3));
    gpio_set_direction(4, GPIO_MODE_OUTPUT);
    gpio_set_direction(13, GPIO_MODE_OUTPUT);
    gpio_set_direction(14, GPIO_MODE_OUTPUT);
    gpio_set_direction(15, GPIO_MODE_OUTPUT);
}

void press_task(void* pvParameters) {
    ESP_LOGI(TAG, "pressing button...");
    int id = (int)pvParameters;
    if (id < 0 || id > 3)return;
    int pin = id + 12;
    if (id == 0)pin = 4;
    gpio_set_level(pin, config_getBtnInvert(id));
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(pin, !config_getBtnInvert(id));
    ESP_LOGI(TAG, "pressed button");
    vTaskDelete(NULL);
}

bool led_status(){
    return gpio_get_level(12);
}

void buttons_press(int id) {
    xTaskCreate(press_task, "btnPress_task", 2048, (void*)id, 5, NULL);
}