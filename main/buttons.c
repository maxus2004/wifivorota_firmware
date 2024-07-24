#include "buttons.h"

#include <driver/gpio.h>
#include <sys/time.h>
#include <esp_log.h>
#include <esp_rom_gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config.h"

static const char* TAG = "WiFIVorota-Buttons";

void buttons_init() {
    esp_rom_gpio_pad_select_gpio(4);
    esp_rom_gpio_pad_select_gpio(13);
    esp_rom_gpio_pad_select_gpio(14);
    esp_rom_gpio_pad_select_gpio(15);
    gpio_set_level(4, 1);
    gpio_set_level(13, 1);
    gpio_set_level(14, 1);
    gpio_set_level(15, 1);
    gpio_set_direction(4, GPIO_MODE_OUTPUT);
    gpio_set_direction(13, GPIO_MODE_OUTPUT);
    gpio_set_direction(14, GPIO_MODE_OUTPUT);
    gpio_set_direction(15, GPIO_MODE_OUTPUT);
}

void press_task(void* pvParameters) {
    ESP_LOGI(TAG, "pressing button...");
    int id = (int)pvParameters;
    if (id < 0 || id > 3) return;
    int pin = id + 12;
    if (id == 0) pin = 4;
    gpio_set_level(pin, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(pin, 1);
    ESP_LOGI(TAG, "pressed button");
    vTaskDelete(NULL);
}

bool led_status() { return !gpio_get_level(12); }

void buttons_press(int id) { xTaskCreate(press_task, "btnPress_task", 2048, (void*)id, 5, NULL); }

void buttons_hold(int id) {
    ESP_LOGI(TAG, "holding button");
    if (id < 0 || id > 3) return;
    int pin = id + 12;
    if (id == 0) pin = 4;
    gpio_set_level(pin, 0);
}

void buttons_release(int id) {
    ESP_LOGI(TAG, "releasing button");
    if (id < 0 || id > 3) return;
    int pin = id + 12;
    if (id == 0) pin = 4;
    gpio_set_level(pin, 1);
}

static int64_t millis() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL));
}

bool buttons_prog(int id) {
    typedef enum { IDLE, WAITING_FOR_SIGNAL, WAITING_FOR_BUTTON, DONE, FAILED } status_t;
    status_t status = IDLE;

    buttons_hold(0);
    vTaskDelay(pdMS_TO_TICKS(100));
    for (int i = 0; i < 4; i++) {
        buttons_hold(1);
        vTaskDelay(pdMS_TO_TICKS(100));
        buttons_release(1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    buttons_release(0);

    status = WAITING_FOR_SIGNAL;

    uint32_t lastFlash = millis();
    while (status == WAITING_FOR_SIGNAL) {
        while (!led_status() && millis() < lastFlash + 3000) {
            vTaskDelay(1);
        }
        uint32_t flashStart = millis();
        uint32_t flashInterval = millis() - lastFlash;
        lastFlash = flashStart;
        while (led_status()) {
            vTaskDelay(1);
        }
        uint32_t flashLength = millis() - flashStart;

        if (flashInterval > 2700 || flashLength > 700) status = FAILED;
        if (flashInterval < 500) status = WAITING_FOR_BUTTON;
    }

    if (status == FAILED) return false;

    buttons_hold(id);
    vTaskDelay(pdMS_TO_TICKS(100));
    buttons_release(id);

    status = DONE;

    return true;
}