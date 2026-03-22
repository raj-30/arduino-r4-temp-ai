**Project:** Smart Temperature Monitoring & Prediction (AI/ML)

**Status:** Phase 2 Complete (Standalone Cloud Logging)

**Date:** February 28, 2026

---

### **1. Introduction**

The objective of Phase 2 was to evolve the data collection system from a laptop-dependent logger (Phase 1) into a fully independent IoT device. The device now operates on wall power, automatically connects to WiFi, fetches outdoor weather data for comparison, and uploads all telemetry to the ThingSpeak cloud for remote visualization.

### **2. Equipment Inventory**

- **Microcontroller:** Arduino UNO R4 WiFi (ESP32-S3 Module)
- **Sensor:** DS18B20 Digital Temperature Sensor (Waterproof/Black Module).
- **Power Source:** Standard USB-C Phone Charger (5V, 2A) or Power Bank.
- **Display:** Built-in 12x8 LED Matrix (Arduino R4 feature).
- **Cloud Platform:** ThingSpeak (Free Tier).
- **Weather API:** OpenWeatherMap (Free Tier).

---

### **3. Final Wiring Configuration**

We standardized on a simple, robust wiring scheme using the Arduino's 5V rail.

| **Sensor Wire** | **Color** | **Arduino Pin** | **Function** |
| --- | --- | --- | --- |
| **Signal** | **Orange** | **Pin 2** | OneWire Data Communication. |
| **VCC (+)** | Green  | **5V** | Main Power Supply. |
| **GND (-)** | **Blue** | **GND** | Common Ground. |

> **Note:** Earlier attempts to power the sensor via Digital Pin 8 (to force resets) were replaced by a software-based bus reset strategy, which proved more reliable and simpler to wire.
> 

---

### **4. Challenges Faced & Solutions (Troubleshooting Log)**

### **Issue A: The "Frozen" Sensor (Reading 28.81°C Constantly)**

- **Symptom:** The DS18B20 sensor would return a single value (e.g., 28.81) and never update, even when heated.
- **Root Cause:** The Arduino R4's high speed and complex WiFi interrupts interfered with the standard `DallasTemperature` library, causing the sensor bus to hang.
- **Solution:** We abandoned the library for the reading process and wrote a **Raw OneWire Function**. This function explicitly resets the bus (`ds.reset()`) and forces a new conversion (`ds.write(0x44)`) before every single reading.
- **Lesson:** For critical sensors, "Active Verification" (resetting the state manually) is safer than relying on library defaults.

### **Issue B: The "Silent" Wall Charger**

- **Symptom:** The device worked on a laptop but refused to connect or run when plugged into a USB wall charger.
- **Root Cause:** The code contained the line `while (!Serial);`. This command pauses the Arduino until a computer opens the Serial Monitor. Since a wall charger cannot open a monitor, the code froze forever.
- **Solution:** We removed all blocking Serial commands. The device now runs immediately upon receiving power.

### **Issue C: Connection Drops (Error -301)**

- **Symptom:** The device would upload data for ~2 minutes and then stop with "Connection Failed."
- **Root Cause:**
    1. **Stale Clients:** The global `WiFiClient` variable was keeping old connections open until they timed out.
    2. **Signal Drop:** Temporary WiFi fluctuations caused the Arduino to disconnect, with no logic to reconnect.
- **Solution:**
    1. **Local Clients:** We moved `WiFiClient` inside the upload function, forcing a fresh "handshake" for every single upload.
    2. **Auto-Reconnect:** We added a `checkWiFi()` function that runs before every upload. If the WiFi is down, it automatically attempts to reconnect.

### **Issue D: Compilation Errors (`const` mismatch)**

- **Symptom:** The LED Matrix library threw errors like `invalid conversion from const uint8_t* to uint8_t*`.
- **Solution:** We removed the `const` keyword from our icon bitmap definitions, allowing the strict R4 library to accept the data.

---

### **5. The "Master Code" (Final Solution)**

This code integrates all solutions: Raw Sensor reading, Auto-WiFi reconnection, Manual HTTP uploading, and Visual feedback.

**Upload this to your Arduino R4 WiFi:**

```cpp
/*
  MASTER CODE: STANDALONE IOT MONITOR
  - Feature 1: Auto-Reconnects WiFi if signal drops.
  - Feature 2: Raw OneWire Reset fixes "Frozen Temp" issues.
  - Feature 3: Manual HTTP Request fixes ThingSpeak timeouts.
  - Feature 4: Visual Icons for Headless Operation (Wall Power).
*/

#include <WiFiS3.h>
#include <OneWire.h>
#include <ArduinoGraphics.h>
#include <Arduino_LED_Matrix.h>
#include <Arduino_JSON.h>

// --- CONFIGURATION ---
char ssid[] = "OPPO Reno8 5G";
char pass[] = "123453354";

// ThingSpeak & OpenWeatherMap Keys
String myWriteAPIKey = "OTBH361SK9B1HUEY"; 
String weatherApiKey = "b9889ad2a30f0ae4eadaf76331bce8eb";
String city = "Mahesāna";
String countryCode = "IN";

// --- HARDWARE ---
OneWire ds(2); // Sensor on Pin 2
ArduinoLEDMatrix matrix;

// --- ICONS (0=OFF, 1=ON) ---
uint8_t icon_wifi[8][12] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0},
  {0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0},
  {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0},
  {0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0},
  {0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
uint8_t icon_check[8][12] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0},
  {0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0},
  {0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0},
  {0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0}
};
uint8_t icon_upload[8][12] = {
  {0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0},
  {0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0},
  {0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0},
  {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0}
};
uint8_t icon_error[8][12] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0},
  {0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0},
  {0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0},
  {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0},
  {0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0},
  {0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0},
  {0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0}
};

// --- TIMERS ---
unsigned long lastDebugTime = 0;
unsigned long lastUploadTime = 0;
const unsigned long debugInterval = 5000;   
const unsigned long uploadInterval = 20000; 

void setup() {
  Serial.begin(115200);
  matrix.begin();

  Serial.println("\n--- SYSTEM STARTING ---");
  matrix.renderBitmap(icon_wifi, 8, 12); 

  checkWiFi(); // Initial Connection
  
  matrix.renderBitmap(icon_check, 8, 12);
  delay(1000);
  matrix.clear();
}

void loop() {
  unsigned long currentMillis = millis();

  // --- TIMER 1: DEBUG LOOP (Every 5s) ---
  if (currentMillis - lastDebugTime >= debugInterval) {
    lastDebugTime = currentMillis;

    float currentTemp = getRawTemperature();
    Serial.print("[DEBUG 5s] Temp: "); Serial.println(currentTemp);
    displayTemp(currentTemp);
  }

  // --- TIMER 2: CLOUD LOOP (Every 20s) ---
  if (currentMillis - lastUploadTime >= uploadInterval) {
    lastUploadTime = currentMillis;
    
    // VERIFICATION: Check WiFi before trying
    checkWiFi();
    
    Serial.println("\n[CLOUD 20s] Starting Upload...");
    matrix.renderBitmap(icon_upload, 8, 12); 

    // 1. Get Data
    float indoorTemp = getRawTemperature();
    float outdoorTemp = 0.0;
    int humidity = 0;
    getWeatherData(outdoorTemp, humidity);

    // 2. Upload
    uploadToThingSpeak(indoorTemp, outdoorTemp, humidity);
    
    Serial.println("--------------------------------------");
  }
}

// --- CORE FUNCTIONS ---

// 1. RAW TEMPERATURE READER (Fixes Frozen Temp)
float getRawTemperature() {
  byte data[12];
  ds.reset();
  ds.write(0xCC); // Skip ROM
  ds.write(0x44); // Start Convert
  delay(800);     // Wait for conversion
  
  ds.reset();
  ds.write(0xCC);
  ds.write(0xBE); // Read Scratchpad

  for ( int i = 0; i < 9; i++) {           
    data[i] = ds.read();
  }
  int16_t raw = (data[1] << 8) | data[0];
  float celsius = (float)raw / 16.0;
  
  if (celsius == 85.00 || celsius == -127.00) return -127.00;
  return celsius;
}

// 2. AUTO-RECONNECT WIFI
void checkWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("\n[WiFi] Lost! Reconnecting");
  matrix.renderBitmap(icon_wifi, 8, 12);
  
  WiFi.disconnect(); 
  WiFi.begin(ssid, pass);
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) Serial.println("\n[WiFi] Reconnected!");
}

// 3. MANUAL UPLOAD (Fixes -301 Error)
void uploadToThingSpeak(float tempIn, float tempOut, int hum) {
  WiFiClient tsClient; // Local Client = Fresh Connection
  const char* host = "api.thingspeak.com";
  
  if (tsClient.connect(host, 80)) {
    String url = "/update?api_key=" + myWriteAPIKey;
    url += "&field1=" + String(tempIn);
    url += "&field2=" + String(tempOut);
    url += "&field3=" + String(hum);
    
    tsClient.print("GET " + url + " HTTP/1.1\r\n");
    tsClient.print("Host: " + String(host) + "\r\n");
    tsClient.print("Connection: close\r\n\r\n");
    
    unsigned long timeout = millis();
    while (tsClient.available() == 0) {
      if (millis() - timeout > 5000) {
        tsClient.stop();
        matrix.renderBitmap(icon_error, 8, 12);
        return;
      }
    }
    Serial.println(" -> [SUCCESS] Sent.");
    matrix.renderBitmap(icon_check, 8, 12);
    delay(1000);
  } else {
    matrix.renderBitmap(icon_error, 8, 12);
  }
  tsClient.stop();
}

void getWeatherData(float &outTemp, int &outHum) {
  WiFiClient weatherClient; 
  if (weatherClient.connect("api.openweathermap.org", 80)) {
    weatherClient.println("GET /data/2.5/weather?q=" + city + "," + countryCode + "&units=metric&appid=" + weatherApiKey + " HTTP/1.1");
    weatherClient.println("Host: api.openweathermap.org");
    weatherClient.println("Connection: close");
    weatherClient.println();

    while (weatherClient.connected()) {
      String line = weatherClient.readStringUntil('\n');
      if (line == "\r") break;
    }
    String payload = weatherClient.readString();
    JSONVar myObject = JSON.parse(payload);

    if (JSON.typeof(myObject) != "undefined" && myObject.hasOwnProperty("main")) {
      outTemp = (double)myObject["main"]["temp"];
      outHum = (int)myObject["main"]["humidity"];
    }
  }
  weatherClient.stop();
}

void displayTemp(float temp) {
  String text = (temp == -127.00) ? "Err" : String(temp, 1);
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(60); 
  matrix.textFont(Font_5x7);
  matrix.beginText(0, 1, 0xFFFFFFFF); 
  matrix.print(text);
  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();
}
```

### **6. Conclusion & Next Steps**

This Phase 2 system is now stable and capable of running for days without intervention. The self-healing logic ensures data continuity even if the network fluctuates.

**Next Steps (Phase 3):**

1. **Data Accumulation:** Let the device run for 48 hours to collect Day/Night cycles.
2. **Export:** Download the CSV from ThingSpeak.
3. **Machine Learning:** Use the dataset to train the Linear Regression Model for indoor temperature prediction.