#ifndef CONFIG_H
#define CONFIG_H

// WiFi — copy this file to config_local.h and fill in your values
// config_local.h is in .gitignore and will never be committed

#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

// IP address of the machine running server.py
// Find it with: ipconfig (Windows) or ip addr (Linux)
#define WS_SERVER_HOST  "192.168.x.x"
#define WS_SERVER_PORT  8765

// Modules — disable to save resources
#define ENABLE_CAMERA     1
#define ENABLE_AUDIO_IN   1   // Microphone
#define ENABLE_AUDIO_OUT  1   // Speaker
#define ENABLE_DISPLAY    1   // OLED eyes
#define ENABLE_TOUCH      1

// Pin definitions — Freenove ESP32-S3 WROOM
// OLED (I2C)
#define PIN_OLED_SDA  36
#define PIN_OLED_SCL  37

// Microphone INMP441 (I2S RX)
#define PIN_I2S_MIC_SCK  42
#define PIN_I2S_MIC_WS   41
#define PIN_I2S_MIC_SD   40

// Amplifier MAX98357A (I2S TX)
#define PIN_I2S_AMP_BCLK  14
#define PIN_I2S_AMP_LRC   21
#define PIN_I2S_AMP_DIN   47

// Touch sensor TTP223B
#define PIN_TOUCH_SIG  48

#endif
