**Project:** Smart Temperature Monitoring & Prediction (AI/ML)

**Status:** Phase 1 Complete (Data Pipeline Operational)

**Last Updated:** February 28, 2026

---

### **1. Executive Summary**

We have successfully built and deployed an IoT data collection node using the Arduino R4 WiFi. The system captures real-time indoor temperature, synchronizes it with online outdoor weather data (via OpenWeatherMap), and logs the dataset to a host computer in CSV format. This creates the foundational dataset required for Phase 2 (Model Training).

### **2. Hardware Configuration**

- **Microcontroller:** Arduino UNO R4 WiFi (ESP32-S3 Architecture).
- **Sensor:** DS18B20 Digital Temperature Sensor (Waterproof/Black Module).
- **Connectivity:** WiFi (2.4GHz Band via Mobile Hotspot).
- **Display:** On-board 12x8 LED Matrix.

### **3. Final Wiring Diagram (Corrected)**

We have standardized the power source to the onboard 5V rail.

| **Sensor Wire** | **Color** | **Arduino Pin** | **Function** |
| --- | --- | --- | --- |
| **Signal** | **Orange** | **Pin 2** | OneWire Data Communication. |
| **VCC (+)** | green | **5V** | Main Power Supply. |
| **GND (-)** | **Blue** | **GND** | Common Ground. |

> **Note:** The "Pin 8 Power Cycle" method discussed during troubleshooting was deprecated in favor of a stable 5V connection with improved software timing.
> 

---

### **4. Challenges & Solutions Log**

### **Issue A: The "Frozen" Sensor Reading (28.81°C)**

- **Problem:** The sensor initially returned a static value and refused to update.
- **Analysis:** The Arduino R4's fast processor speed caused timing conflicts with the sensor's conversion process.
- **Solution:** We implemented `sensors.setWaitForConversion(true)` in the firmware. This forces the processor to pause and wait for the physical sensor to finish measuring before reading the data, ensuring accuracy without needing complex hardware switching.

### **Issue B: The "Silent" Data Stream**

- **Problem:** Python connected to the port but received no data.
- **Solution:** We diagnosed this as a "Flow Control" mismatch. The Arduino R4 requires specific DTR/RTS signals to open the Serial bridge. The Python script was updated with `dsrdtr=True` and `rtscts=True` to auto-trigger the connection.

### **Issue C: SSL/HTTPS Failure**

- **Problem:** The API returned `0.00` for outdoor temperature.
- **Solution:** The secure HTTPS handshake failed on the microcontroller. We switched the API request to **Port 80 (HTTP)**, effectively bypassing the encryption overhead and fixing the data retrieval.

---

### **5. Final Firmware Code (v1.5 - Standard 5V)**

*This code removes the Pin 8 logic and relies on standard 5V power.*

```c
#include <WiFiS3.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Arduino_JSON.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoGraphics.h>
#include <Arduino_LED_Matrix.h>

// --- CONFIGURATION ---
char ssid[] = "OPPO Reno8 5G"; 
char pass[] = "123453354";
String apiKey = "b9889ad2a30f0ae4eadaf76331bce8eb";
String city = "Ahmedabad";
String countryCode = "IN";

// --- PINS ---
#define ONE_WIRE_BUS 2  // Signal Pin (Orange)

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
ArduinoLEDMatrix matrix;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800);
WiFiClient client; 

unsigned long lastTime = 0;
unsigned long timerDelay = 5000; // 5 Seconds Interval

void setup() {
  Serial.begin(115200);
  
  // 1. Hardware Init
  matrix.begin();
  sensors.begin();
  
  // CRITICAL: Solves the "Frozen" 28.81 issue on 5V
  sensors.setWaitForConversion(true); 
  
  // 2. WiFi Connection
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, pass);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    timeClient.begin();
  }
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    
    // --- STEP 1: READ SENSOR ---
    sensors.requestTemperatures(); // Pauses here for conversion
    float indoorTemp = sensors.getTempCByIndex(0);

    // --- STEP 2: GET ONLINE DATA ---
    unsigned long epochTime = 0;
    String formattedTime = "00:00:00";
    float outdoorTemp = 0.0;
    int humidity = 0;

    if (WiFi.status() == WL_CONNECTED) {
      timeClient.update();
      epochTime = timeClient.getEpochTime();
      formattedTime = timeClient.getFormattedTime();
      getWeatherData(outdoorTemp, humidity);
    }

    // --- STEP 3: OUTPUT CSV ---
    Serial.print(epochTime); Serial.print(",");
    Serial.print(formattedTime); Serial.print(",");
    Serial.print(indoorTemp); Serial.print(",");
    Serial.print(outdoorTemp); Serial.print(",");
    Serial.println(humidity);

    // --- STEP 4: DISPLAY ---
    displayOnMatrix(indoorTemp);
    
    lastTime = millis();
  }
}

// Helper to fetch weather
void getWeatherData(float &outTemp, int &outHum) {
  if (client.connect("api.openweathermap.org", 80)) {
    client.println("GET /data/2.5/weather?q=" + city + "," + countryCode + "&units=metric&appid=" + apiKey + " HTTP/1.1");
    client.println("Host: api.openweathermap.org");
    client.println("Connection: close");
    client.println();
    
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") break;
    }
    String payload = client.readString();
    JSONVar myObject = JSON.parse(payload);
    if (JSON.typeof(myObject) != "undefined") {
      outTemp = (double)myObject["main"]["temp"];
      outHum = (int)myObject["main"]["humidity"];
    }
  }
}

void displayOnMatrix(float temp) {
  String text = " " + String(temp, 1) + "C ";
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

## Python data collector script

```python
import serial
import csv
import time
import os

# --- CONFIGURATION ---
PORT = "COM10"         # Confirmed working
BAUD = 115200
FILE_NAME = "dataset_ahmedabad.csv"

print(f"--- STARTING DATA LOGGING ON {PORT} ---")

try:
    # 1. Open Connection (With the "Force Open" settings that worked)
    ser = serial.Serial(PORT, BAUD, timeout=2, dsrdtr=True, rtscts=True)
    
    # 2. Reset the Board (The "Kick")
    ser.dtr = False
    ser.rts = False
    time.sleep(0.5)
    ser.dtr = True
    ser.rts = True
    time.sleep(2)  # Wait for WiFi to reconnect

    # 3. Create CSV if needed
    if not os.path.exists(FILE_NAME):
        with open(FILE_NAME, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["Epoch", "Time", "Indoor_Temp", "Outdoor_Temp", "Humidity"])
            print(f"Created new file: {FILE_NAME}")

    print("Listening... (Data will appear below AND in the CSV file)")
    print("-" * 50)

    while True:
        if ser.in_waiting > 0:
            try:
                # Read and clean the line
                line = ser.readline().decode('utf-8').strip()
                
                # Filter: Only save lines that start with a number (Timestamp)
                if line and line[0].isdigit():
                    print(f"[SAVED]: {line}")
                    
                    # Append to CSV
                    with open(FILE_NAME, "a", newline="") as f:
                        f.write(line + "\n")
                else:
                    # Print status messages (Connecting, WiFi Success, etc.)
                    print(f"[STATUS]: {line}")
                    
            except Exception as e:
                print(f"[Ignored Error]: {e}")
        
        time.sleep(0.05)

except KeyboardInterrupt:
    print("\nStopped.")
except Exception as e:
    print(f"\n[ERROR] {e}")
```

### **6. Visualization Dashboard**

![image.png](attachment:c7bbedd3-d954-4d2f-a198-c52eeb2ad47c:image.png)