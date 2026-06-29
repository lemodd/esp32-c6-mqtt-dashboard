# ESP32-C6 MQTT Sensor Dashboard

An MQTT sensor data monitoring dashboard based on **ESP32-C6 + 1.3-inch ST7789 LCD (240x240)**. It receives JSON-formatted sensor data via MQTT TLS protocol, displays temperature, humidity, and barometric pressure in real-time, and supports line chart history, SD card data logging, and yesterday comparison overlay.

---

## 📸 Preview

| Dashboard                        | Temperature Chart                |
|:--------------------------------:|:--------------------------------:|
| ![screen-1](images/screen-1.png) | ![screen-2](images/screen-2.png) |
| **Humidity Chart**               | **Pressure Chart**               |
| ![screen-3](images/screen-3.png) | ![screen-4](images/screen-4.png) |
| **System Info**                  |                                  |
| ![screen-5](images/screen-5.png) |                                  |

---

## ✨ Features

### Core

- **MQTT TLS encrypted** - Secure MQTT connection over port 8883 via WiFiClientSecure
- **JSON parsing** - ArduinoJson-based sensor data parsing
- **Auto-scaling Y-axis** - Adapts to data range automatically, small fluctuations remain visible
- **Yesterday comparison** - Grey dashed line overlay showing yesterday's same-period data

### 5 Screens (cycle via button)

| #   | Name            | Description                                    |
| --- | --------------- | ---------------------------------------------- |
| 1   | **Dashboard**   | 2×2 grid (Temp/Humi/Baro/Time)                 |
| 2   | **Temperature** | Full-screen temperature line chart + yesterday |
| 3   | **Humidity**    | Full-screen humidity line chart + yesterday    |
| 4   | **Pressure**    | Full-screen barometric chart + yesterday       |
| 5   | **System Info** | Device information panel                       |

### Button Controls

| Action                    | Function       | Details                           |
| ------------------------- | -------------- | --------------------------------- |
| **Short press** BOOT      | Switch screen  | 50ms~1s, cycles through 5 screens |
| **Long press** BOOT (>1s) | Rotate display | 0° → 90° → 180° → 270° loop       |
| **Auto-switch**           | Every 10s      | Hands-free cycling                |

### Display

- **Real-time Min/Max** - Each chart shows historical min/max values
- **Connection indicator** - Title color: blue = connected, red = disconnected
- **Current value** - Latest reading shown above each chart

### Data Logging

- **SD card CSV** - Automatically appends sensor data to SD card
- **Boot history loading** - Loads last 50 records from SD on startup to populate charts
- **Timestamp dedup** - Skips duplicate timestamps
- **CSV format** - `sensor_data.csv` with header `timestamp,temp,humi,baro`

### Networking

- **Multi-WiFi fallback** - Scans for known networks, connects by priority list
- **MQTT auto-reconnect** - Automatic detection and reconnection with throttle
- **Retry throttling** - WiFi retry every 10s, MQTT every 5s

### Misc

- **Dark theme UI** - GitHub-dark inspired (#0D1117 background)
- **Screen rotation** - 0°/90°/180°/270° via long press
- **Backlight control** - PWM-based brightness adjustment

---

## 🛠 Hardware Requirements

| Component | Model            | Notes                                  |
| --------- | ---------------- | -------------------------------------- |
| MCU       | **ESP32-C6**     | WiFi 6 + BLE 5, RISC-V                 |
| Display   | **1.3" ST7789**  | 240×240, SPI interface                 |
| Button    | **BOOT (GPIO9)** | Screen switching & rotation            |
| SD Card   | **SPI mode**     | Optional, for data logging (CS: GPIO4) |

---

## 📌 Pin Connections

```
ST7789 LCD:
  SCLK  → GPIO7    (SPI Clock)
  MOSI  → GPIO6    (SPI Data)
  MISO  → GPIO5    (SPI MISO)
  CS    → GPIO14   (Chip Select)
  DC    → GPIO15   (Data/Command)
  RST   → GPIO21   (Reset)
  BLK   → GPIO22   (Backlight, PWM)

SD Card Module:
  CS    → GPIO4    (Chip Select)
  MOSI  → GPIO6    (shared SPI bus)
  MISO  → GPIO5    (shared SPI bus)
  SCLK  → GPIO7    (shared SPI bus)
```

---

## 📂 Project Structure

```
mqtt_helper/
├── mqtt_helper.ino        # Main program (UI, MQTT, WiFi, Loop)
├── config.h               # WiFi/MQTT/Hardware config (edit this)
├── Display_ST7789.cpp     # ST7789 LCD driver (SPI, init sequence, backlight, rotation)
├── Display_ST7789.h       # LCD pin definitions & declarations
├── LVGL_Driver.cpp        # LVGL graphics driver (flush, buffer, timer)
├── LVGL_Driver.h          # LVGL config & declarations
├── SD_Card.h              # SD card driver (init, CSV I/O, history, date utils)
├── mqtt_.txt              # MQTT config notes
└── README.md              # Chinese README
└── README.en.md           # This file
```

---

## ⚙️ Configuration

### WiFi (`config.h`)

Supports multiple networks, connects by priority order:

```c
static const WifiConfig WIFI_LIST[] = {
    { "Home_WiFi",     "your-password" },    // Priority 1
    { "Phone_Hotspot", "your-password" },    // Priority 2
    { "Office_WiFi",   "your-password" },    // Priority 3
};
```

Connection strategy: Scan → match against WIFI_LIST → connect first match → 5s timeout per network.

### MQTT (`config.h`)

```c
#define MQTT_SERVER   "your-mqtt.example.com"     // Broker address
#define MQTT_PORT     8883                         // TLS port
#define MQTT_USER     "your-username"
#define MQTT_PASS     "your-password"
#define MQTT_TOPIC    "esp32/office"               // Data subscription topic
#define MQTT_STATUS   "esp32/state"                // Status publish topic
```

### Hardware (`config.h`)

```c
#define BOOT_BTN        9      // BOOT button GPIO
#define CHART_POINT_NUM 50     // Chart data point count
#define HISTORY_NUM     50     // Boot-time SD history entries to load
#define SD_CS           4      // SD card chip select
```

---

## 📡 MQTT Data Format

Device subscribes to `MQTT_TOPIC` (default `esp32/office`), expects JSON:

```json
{
    "temp": 29.52,
    "humi": 50.9,
    "baro": 995.26,
    "time_stamp": "2026-06-21_10:36:55"
}
```

| Field        | Type   | Unit | Description                       |
| ------------ | ------ | ---- | --------------------------------- |
| `temp`       | float  | °C   | Temperature                       |
| `humi`       | float  | %    | Humidity                          |
| `baro`       | float  | hPa  | Barometric pressure               |
| `time_stamp` | string | -    | Timestamp (`YYYY-MM-DD_HH:MM:SS`) |

On connect, device publishes `online` to `MQTT_STATUS` topic (default `esp32/state`).

---

## 🖥 Screen Details

### Screen 1: Dashboard

2×2 grid layout with four cards showing live data:

![screen-1](images/screen-1.png)

### Screens 2-4: Full-screen Sensor Charts

Each sensor has its own dedicated full-screen chart:

| ![screen-2](images/screen-2.png) | ![screen-3](images/screen-3.png) | ![screen-4](images/screen-4.png) |
|:--------------------------------:|:--------------------------------:|:--------------------------------:|
| Temperature                      | Humidity                         | Pressure                         |

**Auto-scaling Y-axis rules:**

| Sensor   | Condition      | Margin         | Example                       |
| -------- | -------------- | -------------- | ----------------------------- |
| Temp     | Any            | ±1°C           | 29.8~30.2 → 29~31             |
| Humidity | Range < 2%     | ±0.5%          | 60.5~61.2 → 60~61.5           |
| Humidity | Range < 5%     | ±1%            | 58~62 → 57~63                 |
| Humidity | Range ≥ 5%     | ±2%            | 50~60 → 48~62                 |
| Pressure | Any            | ±1 hPa         | 995~997 → 994~998             |
| All      | With yesterday | Combined range | Ensures comparison visibility |

### Screen 5: System Info

![screen-5](images/screen-5.png)

---

## 💾 SD Card Data Logging

### CSV File Format

File: `/sensor_data.csv`

```csv
timestamp,temp,humi,baro
2026-06-21_10:36:55,29.52,50.9,995.26
2026-06-21_10:37:00,29.48,51.2,995.30
2026-06-21_10:37:05,29.55,50.7,995.22
```

### Boot-time History Loading

- On startup, automatically reads last `HISTORY_NUM` records from SD card
- Pre-populates all 3 line charts
- Charts display historical data immediately, no blank waiting

### Yesterday Comparison Overlay

- On first MQTT message received, loads yesterday's data for the same date range
- Displayed as a grey line overlay on each chart
- Y-axis range considers both today's and yesterday's data for proper comparison

---

## 🔄 Screen Navigation & Button Controls

```
┌──────────────────────────────────────────────────┐
│                 Button Interaction                │
│                                                    │
│  Short press (<1s) → Switch screen                 │
│                Dashboard → Temp → Humi → Baro →    │
│                SysInfo → Dashboard → ...           │
│                                                    │
│  Long press (>1s) → Rotate display                 │
│                0° → 90° → 180° → 270° → 0°        │
│                                                    │
│  10s idle → Auto-switch to next screen             │
└──────────────────────────────────────────────────┘
```

---

## 🚀 Compile & Upload

### Method 1: Arduino IDE

1. Install **ESP32 Arduino Core** (esp32:esp32, v3.3.10+)
2. Install libraries:
   - `PubSubClient` (by Nick O'Leary, v2.8+)
   - `ArduinoJson` (by Benoit Blanchon, v7.4.3+)
   - `LVGL` (v8.3.x)
3. Configure `lv_conf.h`:

   ```c
   // Enable these fonts
   #define LV_FONT_MONTSERRAT_12  1
   #define LV_FONT_MONTSERRAT_14  1
   #define LV_FONT_MONTSERRAT_16  1
   #define LV_FONT_MONTSERRAT_20  1
   #define LV_FONT_MONTSERRAT_24  1
   #define LV_FONT_MONTSERRAT_28  1
   
   // Disable demos
   #define LV_USE_DEMO_WIDGETS    0
   #define LV_USE_DEMO_BENCHMARK  0
   #define LV_USE_DEMO_MUSIC      0
   #define LV_BUILD_EXAMPLES      0
   ```
4. Board: **ESP32C6 Dev Module**
5. Partition scheme: **Huge APP (3MB No OTA/1MB SPIFFS)**
6. First-time flash: Hold BOOT → press RST → release BOOT

### Method 2: arduino-cli

```bash
# Compile
arduino-cli compile --fqbn "esp32:esp32:esp32c6:PartitionScheme=huge_app" mqtt_helper.ino

# Upload
arduino-cli upload --fqbn "esp32:esp32:esp32c6:PartitionScheme=huge_app" --port COM3 mqtt_helper.ino
```

---

## 📦 Dependencies

| Library            | Version | Description               |
| ------------------ | ------- | ------------------------- |
| ESP32 Arduino Core | 3.3.10+ | ESP32-C6 support          |
| PubSubClient       | 2.8+    | MQTT client               |
| ArduinoJson        | 7.4.3+  | JSON parsing              |
| LVGL               | 8.3.x   | Embedded graphics library |

---

## 🔧 Customization

### Adding a New Sensor

Define a new topic in `config.h`, parse the new field in `mqttCallback`, and create corresponding UI/chart controls.

### Theme Colors

The UI uses a dark theme (`#0D1117` background) with card color (`#1A1A2E`). Sensor accent colors:

| Sensor      | Color  | Hex       |
| ----------- | ------ | --------- |
| Temperature | Red    | `#FF6B6B` |
| Humidity    | Green  | `#6BCB77` |
| Pressure    | Cyan   | `#4ECDC4` |
| Time        | Purple | `#BB86FC` |
| Title       | Blue   | `#58A6FF` |

### Adjusting Chart Data Points

`CHART_POINT_NUM` (in config.h) controls how many data points the line chart displays. Increasing this shows a longer time range but consumes more memory.

---



## 📜 License

This project is for personal learning and reference only.

---

## 🙏 Credits

- [LVGL](https://lvgl.io/) - Open-source embedded GUI library
- [PubSubClient](https://github.com/knolleary/pubsubclient) - MQTT client library
- [ArduinoJson](https://arduinojson.org/) - JSON parsing library
