# Pinout - Eva Robot

Connection guide for **Freenove ESP32-S3 WROOM**.

## Components

### OLED 1.3" (SSD1306/SH1106)
- **SDA**: GPIO36
- **SCL**: GPIO37

### INMP441 Microphone (I2S)
- **SCK**: GPIO42
- **WS**: GPIO41
- **SD**: GPIO40
- **L/R**: GND

### MAX98357A Amplifier (I2S)
- **BCLK**: GPIO14
- **LRC**: GPIO21
- **DIN**: GPIO47

### TTP223B Touch Sensor
- **SIG**: GPIO48

---

## Power & Ground

| Voltage | Source | Connect to |
|---------|--------|------------|
| 3.3V | ESP32 3V3 Pin | OLED, INMP441, TTP223 |
| 5V | ESP32 5V Pin (or External PSU ≥2A) | MAX98357A VIN |
| GND | Any GND Pin | All modules + Speaker (-) |
