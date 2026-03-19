#ifndef CAMERA_MODULE_H
#define CAMERA_MODULE_H

#include "esp_camera.h"
#include "camera_pins.h"

class CameraModule {
public:
    bool initialized = false;

    bool begin() {
        camera_config_t config;
        config.ledc_channel = LEDC_CHANNEL_0;
        config.ledc_timer = LEDC_TIMER_0;
        config.pin_d0 = Y2_GPIO_NUM;
        config.pin_d1 = Y3_GPIO_NUM;
        config.pin_d2 = Y4_GPIO_NUM;
        config.pin_d3 = Y5_GPIO_NUM;
        config.pin_d4 = Y6_GPIO_NUM;
        config.pin_d5 = Y7_GPIO_NUM;
        config.pin_d6 = Y8_GPIO_NUM;
        config.pin_d7 = Y9_GPIO_NUM;
        config.pin_xclk = XCLK_GPIO_NUM;
        config.pin_pclk = PCLK_GPIO_NUM;
        config.pin_vsync = VSYNC_GPIO_NUM;
        config.pin_href = HREF_GPIO_NUM;
        config.pin_sccb_sda = SIOD_GPIO_NUM;
        config.pin_sccb_scl = SIOC_GPIO_NUM;
        config.pin_pwdn = PWDN_GPIO_NUM;
        config.pin_reset = RESET_GPIO_NUM;
        config.xclk_freq_hz = 20000000;
        config.frame_size = FRAMESIZE_VGA; // OV3660 supports higher, but VGA is fast for streaming
        config.pixel_format = PIXFORMAT_JPEG; // for streaming
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.jpeg_quality = 12; // 0-63 lower number means higher quality
        config.fb_count = 1;

        // if PSRAM IC is present, init with UXGA resolution and higher JPEG quality
        if(psramFound()){
            config.jpeg_quality = 10;
            config.fb_count = 2;
            config.grab_mode = CAMERA_GRAB_LATEST;
        } else {
            // Limit to SVGA if no PSRAM
            config.frame_size = FRAMESIZE_SVGA;
            config.fb_location = CAMERA_FB_IN_DRAM;
        }

        esp_err_t err = esp_camera_init(&config);
        if (err != ESP_OK) {
            Serial.printf("Camera init failed with error 0x%x", err);
            return false;
        }

        sensor_t * s = esp_camera_sensor_get();
        // Drop down frame size for higher initial FPS
        if (s->id.PID == OV3660_PID) {
            s->set_vflip(s, 1); // flip it back
            s->set_brightness(s, 1); // up the brightness just a bit
            s->set_saturation(s, -2); // lower the saturation
            
            // Enable advanced quality settings to reduce noise/stripes
            s->set_aec2(s, 1); // Auto Exposure Control
            s->set_awb_gain(s, 1); // Auto White Balance Gain
            s->set_bpc(s, 1); // Black Pixel Correction
            s->set_wpc(s, 1); // White Pixel Correction
            s->set_lenc(s, 1); // Lens Energy Correction
            s->set_dcw(s, 1); // Dummy Clock Wait (helps stability)
        }
        
        initialized = true;
        return true;
    }

    void stop() {
        if(initialized) {
            esp_camera_deinit();
            initialized = false;
        }
    }

    camera_fb_t* capture() {
        if (!initialized) return NULL;
        return esp_camera_fb_get();
    }

    void release(camera_fb_t* fb) {
        if (initialized && fb) {
            esp_camera_fb_return(fb);
        }
    }
};

#endif
