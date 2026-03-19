#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <driver/i2s.h>
#include "config.h"
#include "Eyes.h"
#include "Camera.h"

using namespace websockets;

WebsocketsClient client;
Eyes* eyes = nullptr;
CameraModule* camera = nullptr;

#define DMA_BUF_COUNT 8
#define DMA_BUF_LEN 1024
int16_t audioInBuf[DMA_BUF_LEN];
uint8_t audioOutBuf[DMA_BUF_LEN * 2];

bool isConnected = false;
bool micEnabled = true;
bool speakerEnabled = true;
bool cameraEnabled = true;
unsigned long lastMicSendTime = 0;
int micGainShift = 2;
float speakerVolume = 4.0;

void setupI2S() {
    i2s_config_t i2s_config_tx = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, 
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pin_config_tx = {
        .bck_io_num = PIN_I2S_AMP_BCLK,
        .ws_io_num = PIN_I2S_AMP_LRC,
        .data_out_num = PIN_I2S_AMP_DIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    if (ENABLE_AUDIO_OUT) {
        i2s_driver_install(I2S_NUM_0, &i2s_config_tx, 0, NULL);
        i2s_set_pin(I2S_NUM_0, &pin_config_tx);
        i2s_zero_dma_buffer(I2S_NUM_0);
    }

    i2s_config_t i2s_config_rx = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config_rx = {
        .bck_io_num = PIN_I2S_MIC_SCK,
        .ws_io_num = PIN_I2S_MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = PIN_I2S_MIC_SD
    };

    if (ENABLE_AUDIO_IN) {
        i2s_driver_install(I2S_NUM_1, &i2s_config_rx, 0, NULL);
        i2s_set_pin(I2S_NUM_1, &pin_config_rx);
    }
}

void onMessageCallback(WebsocketsMessage message) {
    if (message.isText()) {
        String text = message.data();
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, text);

        if (!error) {
            String type = doc["type"];
            
            if (type == "emotion") {
                String emo = doc["emotion"];
                if (eyes) eyes->setEmotion(emo);
            }
            else if (type == "vision") {
                if (camera && cameraEnabled) {
                    Serial.println("Capturing image...");
                    bool wasMic = micEnabled;
                    micEnabled = false;
                    delay(50); 
                    
                    camera_fb_t* fb = camera->capture();
                    if (fb) {
                        Serial.printf("Image captured: %u bytes\n", fb->len);
                        client.sendBinary((const char*)fb->buf, fb->len);
                        camera->release(fb);
                    } else {
                        Serial.println("Capture failed");
                    }
                    micEnabled = wasMic;
                }
            }
            else if (type == "config") {
                if (doc.containsKey("mic")) micEnabled = doc["mic"];
                if (doc.containsKey("speaker")) speakerEnabled = doc["speaker"];
                if (doc.containsKey("camera")) cameraEnabled = doc["camera"];
                if (doc.containsKey("mic_gain")) micGainShift = doc["mic_gain"];
                if (doc.containsKey("volume")) speakerVolume = doc["volume"];
            }
        }
    } else if (message.isBinary()) {
        if (speakerEnabled && ENABLE_AUDIO_OUT) {
             size_t len = message.length();
             const char* data = message.c_str();
             const int16_t* src = (const int16_t*)data;
             int16_t* dst = (int16_t*)audioOutBuf;
             size_t sampleCount = len / 2;
             if (sampleCount > DMA_BUF_LEN) sampleCount = DMA_BUF_LEN;
             
             for (size_t i=0; i<sampleCount; i++) {
                 int32_t val = (int32_t)(src[i] * speakerVolume);
                 if (val > 32767) val = 32767;
                 else if (val < -32768) val = -32768;
                 dst[i] = (int16_t)val;
             }
             
             size_t bytes_written;
             i2s_write(I2S_NUM_0, (const char*)dst, sampleCount * 2, &bytes_written, portMAX_DELAY);
        }
    }
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if(event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("Connnection Opened");
        isConnected = true;
        client.send("{\"type\":\"hello\"}");
    } else if(event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("Connnection Closed");
        isConnected = false;
    }
}

void setup() {
    Serial.begin(115200);
    
    if (ENABLE_DISPLAY) {
        eyes = new Eyes(PIN_OLED_SDA, PIN_OLED_SCL);
        eyes->begin(PIN_OLED_SDA, PIN_OLED_SCL);
        eyes->setEmotion("happy");
    }

    if (ENABLE_CAMERA) {
        camera = new CameraModule();
        if(camera->begin()) {
            Serial.println("Camera Ready");
        } else {
            Serial.println("Camera Init Failed");
        }
    }

    setupI2S();

    if (ENABLE_TOUCH) {
        pinMode(PIN_TOUCH_SIG, INPUT);
    }

    Serial.print("Connecting to WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20) {
        delay(500);
        Serial.print(".");
        tries++;
    }
    Serial.println("");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi Connected");
        Serial.println(WiFi.localIP());
        
        client.onMessage(onMessageCallback);
        client.onEvent(onEventsCallback);
        String url = "ws://" + String(WS_SERVER_HOST) + ":" + String(WS_SERVER_PORT);
        client.connect(url);
    } else {
        Serial.println("WiFi Failed");
    }
}

void loop() {
    if (client.available()) {
        client.poll();
    } else {
        isConnected = false;
        static unsigned long lastConnectAttempt = 0;
        if (millis() - lastConnectAttempt > 5000) {
            lastConnectAttempt = millis();
            Serial.println("Connection lost. Attempting reconnection...");
            
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("WiFi lost, reconnecting...");
                WiFi.disconnect();
                WiFi.reconnect();
            } else {
                Serial.println("Connecting to WebSocket...");
                String url = "ws://" + String(WS_SERVER_HOST) + ":" + String(WS_SERVER_PORT);
                client.connect(url);
            }
        }
    }

    if (eyes && ENABLE_DISPLAY) {
        eyes->update();
    }

    if (ENABLE_TOUCH) {
        int touchState = digitalRead(PIN_TOUCH_SIG);
        static unsigned long lastDebounceTime = 0;
        static int lastStableState = LOW;
        
        if (touchState != lastStableState) {
            if (millis() - lastDebounceTime > 50) {
                 lastStableState = touchState;
            }
        } else {
            lastDebounceTime = millis();
        }
        
        touchState = lastStableState;
        static int lastProcessedState = LOW;
        static unsigned long pressStartTime = 0;

        if (touchState == HIGH) {
            if (lastProcessedState == LOW) {
                Serial.println("PTT: Pressed");
                pressStartTime = millis();
                if (eyes) eyes->setEmotion("listening");
            }
        } else {
             if (lastProcessedState == HIGH) {
                 unsigned long pressDuration = millis() - pressStartTime;
                 Serial.printf("PTT: Released (Duration: %lu ms)\n", pressDuration);
                 if (eyes) eyes->setEmotion("neutral");
                 if (pressDuration > 200 && isConnected) {
                     client.send("{\"type\":\"end_of_speech\"}");
                 }
             }
        }
        lastProcessedState = touchState;

        if (isConnected && ENABLE_AUDIO_IN && touchState == HIGH && (millis() - pressStartTime > 200)) {
            size_t bytes_read = 0;
            esp_err_t result = i2s_read(I2S_NUM_1, (void*)audioInBuf, sizeof(audioInBuf), &bytes_read, 0); 
            
            if (result == ESP_OK && bytes_read > 0) {
                if (micGainShift > 0) {
                    for (int i = 0; i < bytes_read / 2; i++) {
                        int32_t val = audioInBuf[i] << micGainShift;
                        if (val > 32767) val = 32767;
                        if (val < -32768) val = -32768;
                        audioInBuf[i] = (int16_t)val;
                    }
                }
                client.sendBinary((const char*)audioInBuf, bytes_read);
            }
        } else {
             size_t bytes_read = 0;
             i2s_read(I2S_NUM_1, (void*)audioInBuf, sizeof(audioInBuf), &bytes_read, 0); 
        }
    }
}
