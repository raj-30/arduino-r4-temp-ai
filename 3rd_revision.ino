/*
  MASTER CODE: ACTIVE VERIFICATION
  - Rule 1: Auto-Reconnect WiFi if dropped (Active WiFi Check).
  - Rule 2: Raw OneWire Reset before every reading (Fixes Frozen Temp).
  - Rule 3: Manual HTTP Upload (Fixes Error -301).

  CREDENTIALS: Stored in arduino_secrets.h (not committed to git).
  Copy arduino_secrets.h.template → arduino_secrets.h and fill in your values.
*/

#include <WiFiS3.h>
#include <OneWire.h>
#include <ArduinoGraphics.h>
#include <Arduino_LED_Matrix.h>
#include <Arduino_JSON.h>
#include "arduino_secrets.h"  // WiFi, ThingSpeak & OpenWeatherMap credentials

// --- CONFIGURATION (values come from arduino_secrets.h) ---
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

String myWriteAPIKey = SECRET_TS_KEY;

String weatherApiKey = SECRET_OWM_KEY;
String city          = SECRET_CITY;
String countryCode   = SECRET_COUNTRY;

// --- HARDWARE ---
// Replaced DallasTemperature library with RAW OneWire
OneWire ds(2); // Pin 2

ArduinoLEDMatrix matrix;

// --- ICONS ---
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
  
  // Note: We don't need sensors.begin() anymore because we use Raw Mode

  Serial.println("\n--- SYSTEM STARTING ---");
  matrix.renderBitmap(icon_wifi, 8, 12); 

  checkWiFi(); // Initial Connection
  
  matrix.renderBitmap(icon_check, 8, 12);
  delay(1000);
  matrix.clear();
}

void loop() {
  unsigned long currentMillis = millis();

  // --- TIMER 1: DEBUG LOOP (5s) ---
  if (currentMillis - lastDebugTime >= debugInterval) {
    lastDebugTime = currentMillis;

    // USE RAW FUNCTION
    float currentTemp = getRawTemperature();

    Serial.print("[DEBUG 5s] Temp: ");
    Serial.print(currentTemp);
    Serial.println(" C");

    displayTemp(currentTemp);
  }

  // --- TIMER 2: CLOUD LOOP (20s) ---
  if (currentMillis - lastUploadTime >= uploadInterval) {
    lastUploadTime = currentMillis;
    
    // VERIFICATION 1: Check WiFi
    checkWiFi();
    
    Serial.println("\n[CLOUD 20s] Starting Upload...");
    matrix.renderBitmap(icon_upload, 8, 12); 

    // 1. Get Indoor Temp (Raw Mode)
    float indoorTemp = getRawTemperature();
    
    // 2. Fetch Outdoor Data
    float outdoorTemp = 0.0;
    int humidity = 0;
    getWeatherData(outdoorTemp, humidity);

    // 3. Upload to ThingSpeak
    uploadToThingSpeak(indoorTemp, outdoorTemp, humidity);
    
    Serial.println("--------------------------------------");
  }
}

// --------------------------------------------------------
//   RAW TEMPERATURE READER (The Fix for "Frozen" Temp)
// --------------------------------------------------------
float getRawTemperature() {
  byte data[12];
  
  // 1. Reset Bus & Start Conversion
  ds.reset();
  ds.write(0xCC); // Skip ROM
  ds.write(0x44); // Convert T
  
  // 2. Wait for conversion (Blocking wait is safer for R4 timing)
  delay(800); 
  
  // 3. Reset & Read
  ds.reset();
  ds.write(0xCC);
  ds.write(0xBE); // Read Scratchpad

  for ( int i = 0; i < 9; i++) {           
    data[i] = ds.read();
  }

  // 4. Calculate
  int16_t raw = (data[1] << 8) | data[0];
  float celsius = (float)raw / 16.0;
  
  // 5. Error Filter
  if (celsius == 85.00 || celsius == -127.00) {
    return -127.00; // Return Error flag
  }
  
  return celsius;
}

// --------------------------------------------------------
//   AUTO-RECONNECT FUNCTION
// --------------------------------------------------------
void checkWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return; // Connected, do nothing
  }

  Serial.print("\n[WiFi] Signal Lost! Reconnecting");
  matrix.renderBitmap(icon_wifi, 8, 12);
  
  WiFi.disconnect(); 
  WiFi.begin(ssid, pass);
  
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Reconnected!");
  } else {
    Serial.println("\n[WiFi] Failed. Retrying next loop.");
  }
}

// --------------------------------------------------------
//   MANUAL UPLOAD FUNCTION
// --------------------------------------------------------
void uploadToThingSpeak(float tempIn, float tempOut, int hum) {
  WiFiClient tsClient; // Local client = Fresh connection
  const char* host = "api.thingspeak.com";
  
  Serial.print(" -> Connecting to ThingSpeak... ");
  
  if (tsClient.connect(host, 80)) {
    Serial.println("Connected!");
    
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
        Serial.println(" -> [Error] Client Timeout!");
        tsClient.stop();
        matrix.renderBitmap(icon_error, 8, 12);
        return;
      }
    }
    
    Serial.println(" -> [SUCCESS] Data Sent.");
    matrix.renderBitmap(icon_check, 8, 12);
    delay(1000);
    
  } else {
    Serial.println(" -> [Error] Connection Failed.");
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
      Serial.print(" -> Weather: "); Serial.println(outTemp);
    }
  }
  weatherClient.stop();
}

void displayTemp(float temp) {
  String text;
  if(temp == -127.00) text = "Err";
  else text = String(temp, 1);
  
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(120); 
  matrix.textFont(Font_5x7);
  matrix.beginText(0, 1, 0xFFFFFFFF); 
  matrix.print(text);
  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();
}