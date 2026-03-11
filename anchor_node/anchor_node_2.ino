// ============================================================
//  ANCHOR NODE 2 — Complete Firmware (WITHOUT MPU6050)
//  Zone: Mine_Zone_2  |  ID: A-02
//  TX Interval: 20 sec normal, IMMEDIATE on danger
// ============================================================

#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ─── LoRa ─────────────────────────────────────────────────────
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 26

// ─── OLED (0.96" I2C) ─────────────────────────────────────────
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ─── Zone Config ──────────────────────────────────────────────
const char*  ZONE_SSID = "Mine_Zone_2";  // ← Zone 2
const char*  ZONE_PASS = "safety123";
const String ANCHOR_ID = "A-02";          // ← Anchor 2

// ─── Gas Sensors (ADC1 only — works with WiFi) ───────────────
#define MQ2_PIN   32
#define MQ4_PIN   34
#define MQ135_PIN 35

// ─── DHT11 ────────────────────────────────────────────────────
#define DHTPIN  4
#define DHTTYPE DHT11
DHT_Unified dht(DHTPIN, DHTTYPE);

// ─── MH LDR Light Sensor ──────────────────────────────────────
#define MH_AO_PIN 33   // ADC1
#define MH_DO_PIN 15   // Digital threshold

// ─── Thresholds ───────────────────────────────────────────────
const int MQ2_DANGER   = 2000;
const int MQ4_DANGER   = 2200;
const int MQ135_DANGER = 2500;
const int DARK_ALERT   = 10;

// ─── TX Interval ──────────────────────────────────────────────
const unsigned long NORMAL_INTERVAL = 20000UL;  // 20 seconds

// ─── Calibration ──────────────────────────────────────────────
float mq2_bl = 0, mq4_bl = 0, mq135_bl = 0;
bool  calibrated = false;
unsigned long calStartTime = 0, lastDailyCal = 0;
const unsigned long CAL_DURATION   = 20000UL;
const unsigned long DAILY_INTERVAL = 86400000UL;

// ─── State ────────────────────────────────────────────────────
unsigned long lastTxTime = 0;
bool wasDanger = false;
String lastWorkerZone = "-";
String lastWorkerSOS  = "OK";

// ────────────────────────────────────────────────────────────
void showOLED(float temp, float humid, int mq2, int mq4,
              int lightPct, bool danger, bool darkAlert, int workers) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);   display.println(String(ZONE_SSID));
  display.setCursor(0, 11);
  display.print("T:"); display.print(temp,1); display.print("C ");
  display.print("H:"); display.print(humid,0); display.print("% ");
  display.print("W:"); display.println(workers);
  display.setCursor(0, 22);
  display.print("Gas:"); display.print(mq2);
  display.print(" CH4:"); display.print(mq4);
  display.setCursor(0, 33);
  display.print("Light:"); display.print(lightPct); display.print("%");
  display.print(" Wkr:"); display.println(lastWorkerSOS);
  display.setCursor(0, 50);
  if (darkAlert) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.println(" !! LIGHTS OUT !!    ");
  } else if (danger) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.println(" !! GAS DANGER !!    ");
  } else {
    display.println("   Status: SAFE      ");
  }
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

// ────────────────────────────────────────────────────────────
bool calibrateSensors(int mq2, int mq4, int mq135) {
  if (calibrated && (millis() - lastDailyCal > DAILY_INTERVAL)) {
    calibrated = false; calStartTime = millis();
    mq2_bl = mq4_bl = mq135_bl = 0;
  }
  if (!calibrated) {
    if (millis() - calStartTime < CAL_DURATION) {
      mq2_bl   = (mq2_bl   + mq2)   / 2.0;
      mq4_bl   = (mq4_bl   + mq4)   / 2.0;
      mq135_bl = (mq135_bl + mq135) / 2.0;
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
      display.setCursor(10, 20); display.println("Calibrating...");
      display.setCursor(10, 35); display.println("Please wait 20s");
      display.display();
      LoRa.beginPacket();
      LoRa.print("{\"anchor\":\"" + ANCHOR_ID + "\",\"status\":\"calibrating\"}");
      LoRa.endPacket();
      delay(1000);
      return false;
    } else {
      calibrated = true; lastDailyCal = millis();
      LoRa.beginPacket();
      LoRa.print("{\"anchor\":\"" + ANCHOR_ID + "\",\"status\":\"calibrated\"}");
      LoRa.endPacket();
    }
  }
  return true;
}

// ────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  SPI.begin(); Wire.begin(); dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("OLED not found!");
  else {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 20); display.println("MineGuard Anchor 2");
    display.setCursor(8, 36);  display.println("Zone: " + String(ZONE_SSID));
    display.display(); delay(2000);
  }

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) { Serial.println("LoRa failed!"); while(1); }

  WiFi.softAP(ZONE_SSID, ZONE_PASS);
  pinMode(MH_DO_PIN, INPUT);
  calStartTime = millis();
  Serial.println("Anchor 2 ready: " + String(ZONE_SSID));
}

// ────────────────────────────────────────────────────────────
void loop() {
  int mq2   = analogRead(MQ2_PIN);
  int mq4   = analogRead(MQ4_PIN);
  int mq135 = analogRead(MQ135_PIN);
  if (!calibrateSensors(mq2, mq4, mq135)) return;

  float temperature = 0.0, humidity = 0.0;
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (!isnan(event.temperature)) temperature = event.temperature;
  dht.humidity().getEvent(&event);
  if (!isnan(event.relative_humidity)) humidity = event.relative_humidity;

  // MH Light → 0–100%
  int rawLight = analogRead(MH_AO_PIN);
  int lightPct = constrain(map(rawLight, 4095, 0, 0, 100), 0, 100);
  bool dark_alert = (lightPct < DARK_ALERT) || (digitalRead(MH_DO_PIN) == LOW);

  // Alerts (no vibration on Anchor 2)
  bool gas_alert  = (mq2   > MQ2_DANGER);
  bool meth_alert = (mq4   > MQ4_DANGER);
  bool air_alert  = (mq135 > MQ135_DANGER);
  bool danger     = gas_alert || meth_alert || air_alert || dark_alert;

  int workers = WiFi.softAPgetStationNum();

  // Listen for worker LoRa
  int pktSize = LoRa.parsePacket();
  if (pktSize) {
    String msg = "";
    while (LoRa.available()) msg += (char)LoRa.read();
    lastWorkerSOS = (msg.indexOf("\"sos\":true") >= 0) ? "SOS!" : "OK";
    if (msg.indexOf("\"zone\":\"") >= 0) {
      int idx = msg.indexOf("\"zone\":\"") + 8;
      lastWorkerZone = msg.substring(idx, msg.indexOf("\"", idx));
    }
  }

  // Serial Monitor
  Serial.println("=====================================");
  Serial.print("Zone       : "); Serial.println(ZONE_SSID);
  Serial.print("Workers    : "); Serial.println(workers);
  Serial.println("--- Environment ---");
  Serial.print("Temp       : "); Serial.print(temperature,1); Serial.println(" C");
  Serial.print("Humidity   : "); Serial.print(humidity,1);    Serial.println(" %");
  Serial.println("--- Gas Sensors ---");
  Serial.print("MQ-2 (Gas) : "); Serial.print(mq2);   Serial.println(gas_alert  ? " ALERT!" : " OK");
  Serial.print("MQ-4 (CH4) : "); Serial.print(mq4);   Serial.println(meth_alert ? " ALERT!" : " OK");
  Serial.print("MQ-135 AQI : "); Serial.print(mq135); Serial.println(air_alert  ? " ALERT!" : " OK");
  Serial.println("--- Light ---");
  Serial.print("Light      : "); Serial.print(lightPct); Serial.println(" %");
  Serial.print("Dark Alert : "); Serial.println(dark_alert ? "YES!" : "No");
  Serial.println("--- Worker ---");
  Serial.print("Worker SOS : "); Serial.println(lastWorkerSOS);
  Serial.print("DANGER     : "); Serial.println(danger ? "*** YES ***" : "No");
  Serial.println("=====================================");

  // OLED
  showOLED(temperature, humidity, mq2, mq4, lightPct, danger, dark_alert, workers);

  // TX — 20s normal, IMMEDIATE on danger
  unsigned long now = millis();
  if (danger && !wasDanger) lastTxTime = 0;
  wasDanger = danger;

  if (now - lastTxTime >= (danger ? 0UL : NORMAL_INTERVAL) || lastTxTime == 0) {
    lastTxTime = now;
    String p = "{";
    p += "\"type\":\"env\",";
    p += "\"anchor\":\""  + ANCHOR_ID           + "\",";
    p += "\"zone\":\""    + String(ZONE_SSID)    + "\",";
    p += "\"workers\":"   + String(workers)       + ",";
    p += "\"mq2\":"       + String(mq2)           + ",";
    p += "\"mq4\":"       + String(mq4)           + ",";
    p += "\"mq135\":"     + String(mq135)         + ",";
    p += "\"temp\":"      + String(temperature,1) + ",";
    p += "\"humid\":"     + String(humidity,1)    + ",";
    p += "\"light_pct\":" + String(lightPct)      + ",";
    p += "\"dark_alert\":"+ String(dark_alert  ? "true":"false") + ",";
    p += "\"gas_alert\":" + String(gas_alert   ? "true":"false") + ",";
    p += "\"meth_alert\":"+ String(meth_alert  ? "true":"false") + ",";
    p += "\"air_alert\":" + String(air_alert   ? "true":"false") + ",";
    p += "\"danger\":"    + String(danger      ? "true":"false");
    p += "}";
    LoRa.beginPacket(); LoRa.print(p); LoRa.endPacket();
    Serial.println("TX: " + p);
  }
  delay(100);
}
