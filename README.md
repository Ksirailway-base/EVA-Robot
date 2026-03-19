# Eva — AI Companion Robot

> Voice-controlled AI companion with vision. Runs fully locally on ESP32-S3 + PC. No cloud required.
---

## What it does

Eva is a physical robot companion that listens, sees, thinks, and speaks — entirely offline. You talk to her, she responds in a cloned voice with matching facial expressions on an OLED display. Ask her "what do you see?" and she captures an image with her camera and describes it.

**Full pipeline on one machine:**
- Speech → Whisper STT → Qwen VL (LLM) → F5-TTS → Speaker
- Camera → OpenCV preprocessing → Qwen VL (vision)
- LLM emotion tags → OLED facial expressions

---

## Architecture

```
ESP32-S3                         Python Server (asyncio)
─────────────────────────        ──────────────────────────────────
INMP441 mic (I2S)  ──────────►  Whisper STT  (faster-whisper)
                   WebSocket        │
OV3660 camera      ──────────►  CV2 pipeline (CLAHE + denoise)
                                    │
                                Qwen3-VL  (llama.cpp / GGUF)
                                    │
                                Emotion parser  [HAPPY/SAD/ANGRY]
                                    │
OLED display  ◄──────────────  Emotion tag  ──────────────────────
MAX98357A amp ◄──────────────  F5-TTS  (Voice clone)
```

---

## Hardware

| Component | Part |
|-----------|------|
| MCU | Freenove ESP32-S3 WROOM |
| Camera | OV3660 (onboard) |
| Microphone | INMP441 (I2S) |
| Amplifier | MAX98357A (I2S) |
| Display | SSD1306 / SH1106 OLED 128×64 |
| Touch | TTP223B capacitive sensor |

3D-printed enclosure designed in Autodesk Fusion — model available on *SOON*

---

## Tech stack

`Python` · `asyncio` · `websockets` · `llama.cpp` · `faster-whisper` · `F5-TTS` · `OpenCV` · `pydub` · `C++` · `ESP-IDF (Arduino)`

---

## Quick start

### 1. Server

1. **Clone & Setup**:
   ```cmd
   git clone https://github.com/Ksirailway-base/EVA-Robot.git
   cd EVA-Robot/server
   .\setup.bat
   ```
   This will automatically create a virtual environment, install dependencies, and download the required `llama.cpp` binaries (v.b8429) to `server/bin/`.

2. **Reference Audio**:
   Add your voice sample to `server/ref_audio/custom.wav` (~10 sec) and its transcript to `server/ref_audio/custom.wav.txt`.

3. **Models**:
   Place the GGUF models in the root directory (one level above `server/`):
   - `Qwen3-VL-4B-Instruct-Q4_K_M.gguf`
   - `mmproj-Qwen3VL-4B-Instruct-Q8_0.gguf`

4. **Run**:
   ```cmd
   cd server
   .\run.bat
   ```

### 2. Firmware

1. Copy `firmware/config.h` → `firmware/config_local.h`
2. Fill in your WiFi credentials and server IP in `config_local.h`
3. Open `firmware/firmware.ino` in Arduino IDE
4. Board settings:
   - **Board**: ESP32S3 Dev Module
   - **PSRAM**: OPI PSRAM
   - **Flash Mode**: QIO 80MHz
5. Upload

### 3. Done

Power on the ESP32. It connects to your server over WebSocket. Say something.

---

## Configuration

Edit `server/system_prompt.txt` to change Eva's personality. The emotion tag format (`[HAPPY]`, `[SAD]`, `[ANGRY]`, `[NEUTRAL]`) must be preserved — it drives the OLED expressions.

You can replace the voice by swapping `ref_audio/custom.wav` with any ~10 second clean audio sample and updating the corresponding `.txt` transcript.

---

## Known limitations

- Audio latency is ~2-4s depending on hardware (Whisper + LLM + TTS)
- F5-TTS works best when reference audio and generated text are in the same language

---

## License

MIT
