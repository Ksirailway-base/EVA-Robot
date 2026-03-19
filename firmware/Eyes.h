#ifndef EYES_H
#define EYES_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define I2C_ADDRESS 0x3C // Try 0x3D if 0x3C doesn't work

class Eyes {
public:
    Adafruit_SH1106G display;
    
    enum Emotion {
        NEUTRAL,
        HAPPY,
        SAD,
        ANGRY,
        LISTENING
    };

    Emotion currentEmotion = NEUTRAL;
    
    // Animation State
    float curX = 0;
    float curY = 0;
    float targetX = 0;
    float targetY = 0;
    
    // Blink State
    bool isBlinking = false;
    int blinkState = 0; // 0: Open, 1: Closing, 2: Opening
    float blinkHeight = 1.0; // 1.0 = Full open, 0.0 = Closed
    unsigned long lastBlinkTime = 0;
    unsigned long nextBlinkInterval = 3000;
    
    // Saccade State
    unsigned long lastSaccadeTime = 0;
    unsigned long nextSaccadeInterval = 1000;

    // Eye Config
    const int EYE_W = 32;
    const int EYE_H = 44;
    const int EYE_GAP = 24; // Increased from 16 to 24
    const int EYE_RADIUS = 12;

    Eyes(int sda, int scl) : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET) {
    }

    void begin(int sda, int scl) {
        Wire.begin(sda, scl);
        Wire.setClock(400000);
        
        if(!display.begin(I2C_ADDRESS, true)) { 
            Serial.println(F("SH1106 allocation failed"));
        }
        
        display.clearDisplay();
        display.fillScreen(SH110X_BLACK);
        display.display();
        
        lastBlinkTime = millis();
        randomSeed(analogRead(0));
    }

    void setEmotion(String emotionStr) {
        if (emotionStr == "happy") currentEmotion = HAPPY;
        else if (emotionStr == "sad") currentEmotion = SAD;
        else if (emotionStr == "angry") currentEmotion = ANGRY;
        else currentEmotion = NEUTRAL;
    }

    void update() {
        unsigned long now = millis();
        display.clearDisplay();
        
        // 1. Saccades (Random Eye Movement)
        // Only move if not blinking to avoid conflict? Actually smooth movement is fine during blink.
        if (now - lastSaccadeTime > nextSaccadeInterval) {
            // Pick a new target
            // Limit range to +/- 8 pixels to keep eyes on screen
            targetX = random(-8, 9);
            targetY = random(-5, 6);
            
            lastSaccadeTime = now;
            nextSaccadeInterval = random(500, 3000); // More frequent, lively
        }

        // 2. Smooth Interpolation (Easing)
        // Move 10% of the way to target per frame
        curX += (targetX - curX) * 0.15;
        curY += (targetY - curY) * 0.15;

        // 3. Blink Logic
        if (!isBlinking && (now - lastBlinkTime > nextBlinkInterval)) {
            isBlinking = true;
            blinkState = 1; // Closing
            lastBlinkTime = now;
        }

        if (isBlinking) {
            handleBlinkAnimation();
        }

        // 4. Draw
        drawEyes();
        display.display();
    }

private:
    void handleBlinkAnimation() {
        float speed = 0.15; // Blink speed
        
        if (blinkState == 1) { // Closing
            blinkHeight -= speed;
            if (blinkHeight <= 0.1) {
                blinkHeight = 0.0;
                blinkState = 2; // Start Opening
            }
        } else if (blinkState == 2) { // Opening
            blinkHeight += speed;
            if (blinkHeight >= 1.0) {
                blinkHeight = 1.0;
                isBlinking = false;
                blinkState = 0;
                nextBlinkInterval = random(2000, 6000);
                lastBlinkTime = millis();
            }
        }
    }

    void drawEyes() {
        int centerX = SCREEN_WIDTH / 2;
        int centerY = SCREEN_HEIGHT / 2;
        
        int leftEyeX = centerX - EYE_GAP/2 - EYE_W/2 + (int)curX;
        int rightEyeX = centerX + EYE_GAP/2 + EYE_W/2 + (int)curX;
        int eyesY = centerY + (int)curY;
        
        // Calculate current height based on blink
        int currentH = (int)(EYE_H * blinkHeight);
        if (currentH < 2) currentH = 2; // Avoid 0 height

        // Draw based on Emotion
        switch (currentEmotion) {
            case NEUTRAL:
            case SAD: // Sad handled by shape modification below
            case ANGRY: // Angry handled by shape modification
                drawEyeShape(leftEyeX, eyesY, EYE_W, currentH, currentEmotion, false);
                drawEyeShape(rightEyeX, eyesY, EYE_W, currentH, currentEmotion, true);
                break;
                
            case HAPPY:
                // Happy eyes are typically arcs ^ ^
                // We'll draw them differently
                drawHappyEye(leftEyeX, eyesY, EYE_W, currentH);
                drawHappyEye(rightEyeX, eyesY, EYE_W, currentH);
                break;
        }
    }

    void drawEyeShape(int x, int y, int w, int h, Emotion emo, bool isRight) {
        // x,y is center
        int top = y - h/2;
        int left = x - w/2;
        
        // Base shape: Rounded Rect
        // Adjust radius if height is small to avoid artifacts
        int r = EYE_RADIUS;
        if (h < 2*r) r = h/2;
        
        display.fillRoundRect(left, top, w, h, r, SH110X_WHITE);
        
        // Apply Emotion Masks (Black shapes to cut the white eye)
        
        if (emo == ANGRY) {
            // Angry: Cut top diagonally
            // Slope down towards center
            int cutH = 15;
            if (isRight) {
                // Right eye: Slope /
                display.fillTriangle(left-2, top-2, left+w+2, top-2, left-2, top+cutH, SH110X_BLACK);
            } else {
                // Left eye: Slope \ (backwards)
                display.fillTriangle(left-2, top-2, left+w+2, top-2, left+w+2, top+cutH, SH110X_BLACK);
            }
        } 
        else if (emo == SAD) {
             // Sad: Cut top diagonally (opposite of angry)
             // Slope up towards center? Or just simple flat cut?
             // Usually sad eyes slant like / \ .
             int cutH = 15;
             if (isRight) {
                // Right eye: Slope \ .
                display.fillTriangle(left+w+2, top-2, left-2, top-2, left+w+2, top+cutH, SH110X_BLACK);
             } else {
                 // Left eye: Slope / .
                 display.fillTriangle(left-2, top-2, left+w+2, top-2, left-2, top+cutH, SH110X_BLACK);
             }
        }
    }

    void drawHappyEye(int x, int y, int w, int h) {
        // Draw an arc or just a thick line
        // Since we are using primitives, let's draw a circle and cut the bottom
        // But for "Happy", we want ^ shape.
        // Let's just draw a filled circle and a black circle inside? No, that's an annulus.
        // Let's use fillRoundRect but very short height?
        // Actually, ^ shape is hard with primitives. 
        // Let's do the "inverted U" style: Circle with bottom rect cut.
        
        // If blinking, h goes to 0, which naturally squashes the happy eye too.
        
        int r = w/2;
        // Draw full circle
        display.fillCircle(x, y, r, SH110X_WHITE);
        // Cut the bottom part to make it an arc
        display.fillCircle(x, y+3, r-3, SH110X_BLACK);
        // Cut the bottom half completely to make it an arch
        display.fillRect(x - w/2, y+3, w, h, SH110X_BLACK);
    }
};

#endif
