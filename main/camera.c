#include "camera.h"

#include <esp_camera.h>
#include <esp_log.h>

#include "camera_pins.h"
#include "config.h"

static const char* TAG = "WiFIVorota-Camera";

int quality = 8;
int xclk = 20;
int framesize = FRAMESIZE_VGA;

void camera_start() {
    camera_config_t camera_config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,
        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .xclk_freq_hz = 20000000,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_UXGA,
        .jpeg_quality = 0,
        .fb_count = 2,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY
    };
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) { ESP_LOGE(TAG, "camera init failed"); }
    sensor_t* s = esp_camera_sensor_get();
    s->set_vflip(s, config_getCameraFlip());
    s->set_hmirror(s, config_getCameraFlip());
    s->set_quality(s, quality);
    s->set_xclk(s, LEDC_TIMER_0, xclk);
    s->set_framesize(s, framesize);
}

void camera_setQuality(int value) {
    quality = value;
    sensor_t* s = esp_camera_sensor_get();
    s->set_quality(s, quality);
}
void camera_setXclk(int value) {
    xclk = value;
    sensor_t* s = esp_camera_sensor_get();
    s->set_xclk(s, LEDC_TIMER_0, xclk);
}
void camera_setFramesize(int value) {
    framesize = value;
    sensor_t* s = esp_camera_sensor_get();
    s->set_framesize(s, framesize);
}
int camera_getQuality() { return quality; }
int camera_getXclk() { return xclk; }
int camera_getFramesize() { return framesize; }