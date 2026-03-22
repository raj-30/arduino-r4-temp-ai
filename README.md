# arduino-r4-temp-ai

> Standalone IoT temperature monitor on Arduino UNO R4 WiFi — foundation for an ML-based indoor temperature prediction model.

---

## What This Is

An Arduino UNO R4 WiFi reads indoor temperature every 5 seconds via a DS18B20 sensor, fetches outdoor weather from OpenWeatherMap every 20 seconds, and logs both to ThingSpeak — all running standalone on a USB phone charger with no PC required.

This is **Phase 1** of a larger project. The collected time-series data will be used to train a machine learning model that predicts indoor temperature from outdoor conditions.

---

## Hardware

| Component | Details |
|---|---|
| Microcontroller | Arduino UNO R4 WiFi |
| Sensor | DS18B20 Digital Temperature Sensor |
| Display | Built-in 12×8 LED Matrix (on-board) |
| Power | Any 5V USB-C phone charger or power bank |

### Wiring

| Sensor Wire | Arduino Pin | Function |
|---|---|---|
| Signal | Pin 2 | OneWire data |
| VCC | 5V | Power |
| GND | GND | Ground |

---

## Setup

### 1. Install Libraries

Open Arduino IDE → **Sketch → Include Library → Manage Libraries** and install:

- `OneWire` by Paul Stoffregen
- `ArduinoGraphics` by Arduino
- `Arduino_LED_Matrix` *(bundled with R4 board package)*
- `Arduino_JSON` by Arduino

### 2. Configure Credentials

Copy the template and fill in your own values:

```bash
cp firmware/arduino_secrets.h.template firmware/arduino_secrets.h
```

Edit `arduino_secrets.h`:

```cpp
#define SECRET_SSID        "your_wifi_name"
#define SECRET_PASS        "your_wifi_password"
#define SECRET_TS_KEY      "your_thingspeak_write_key"
#define SECRET_OWM_KEY     "your_openweathermap_api_key"
#define SECRET_CITY        "YourCity"
#define SECRET_COUNTRY     "IN"
```

> `arduino_secrets.h` is listed in `.gitignore` and will never be committed.

### 3. Flash

Open `firmware/3rd_revision.ino` in Arduino IDE, select board **Arduino UNO R4 WiFi** and your COM port, then click **Upload**.

### 4. Run Standalone

Unplug from PC and connect to any USB-C phone charger. The LED matrix will show:

| Icon | Meaning |
|---|---|
| WiFi arcs (blinking) | Connecting to WiFi |
| Checkmark | Connected / upload success |
| Up arrow | Uploading to ThingSpeak |
| X | Error |

---

## Live Data

View real-time graphs on **ThingSpeak** (configure your Channel ID in your secrets file).

- Field 1 — Indoor Temperature (°C)
- Field 2 — Outdoor Temperature (°C)
- Field 3 — Humidity (%)

---

## Firmware Design Notes

Three rules applied throughout the firmware, learned from debugging on the R4:

1. **Always verify WiFi before any network call** — `checkWiFi()` auto-reconnects if the signal drops.
2. **Always reset the OneWire bus before reading** — raw `ds.reset()` prevents the DS18B20 from returning a frozen value on the R4's fast processor.
3. **Always use a local `WiFiClient`** — creating the client inside the upload function guarantees a fresh TCP connection and prevents the `-301` timeout error.

---

## Roadmap

- [x] Phase 1 — Standalone firmware, cloud logging, LED matrix status display
- [ ] Phase 2 — Python data cleaning & exploratory analysis of collected CSV
- [ ] Phase 3 — Jupyter notebook: feature engineering & correlation study
- [ ] Phase 4 — Train and evaluate indoor temperature prediction model (ML)
- [ ] Phase 5 — Deploy lightweight model back to Arduino for on-device inference

---

## License

MIT © Raj Gajjar
