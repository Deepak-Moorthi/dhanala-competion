// ============================================================
//  ANCHOR NODE — Complete Firmware
//  Sensors: LoRa, WiFi-AP, OLED, DHT11, MQ-2/4/135,
//           MPU6050 (vibration), MH LDR (ambient light %)
//
//  *** ADC RULE: Only ADC1 pins (32-39) work with WiFi ON ***
//  GPIOs 25/26/27 are ADC2 — CANNOT analogRead with WiFi
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
#include <Adafruit_MPU6050.h>

// ─── LoRa ────────────────────────────────────────────────────
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 26

// ─── I2C — OLED + MPU6050 (shared GPIO21/22) ─────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_MPU6050 mpu;

// ─── WiFi AP ─────────────────────────────────────────────────
const char*  ZONE_SSID = "Mine_Zone_1"; // ← CHANGE per anchor
const char*  ZONE_PASS = "safety123";
const String ANCHOR_ID = "A-01";        // ← CHANGE per anchor

// ─── Gas Sensors (ADC1 pins — work with WiFi) ────────────────
#define MQ2_PIN   32   // ADC1 — Combustible gas (LPG, CO, smoke)
#define MQ4_PIN   34   // ADC1 — Methane / Natural Gas
#define MQ135_PIN 35   // ADC1 — Air quality (CO2, Ammonia, Benzene)

// ─── DHT11 — Temperature & Humidity ──────────────────────────
#define DHTPIN  4
#define DHTTYPE DHT11
DHT_Unified dht(DHTPIN, DHTTYPE);

// ─── MH Light Sensor (LDR Module) ────────────────────────────
// MH AO: high value = dark, low value = bright (LDR behaviour)
// We invert and map to 0–100% so 100% = full brightness
#define MH_AO_PIN 33   // ADC1 — Analog light level (0–4095)
#define MH_DO_PIN 15   // Digital — LOW when light is below pot threshold

// ─── Gas Alert Thresholds ─────────────────────────────────────
const int MQ2_DANGER   = 2000;
const int MQ4_DANGER   = 2200;
const int MQ135_DANGER = 2500;
const int DARK_ALERT   = 10;   // % — below this = tunnel lights failed

// ─── MPU6050 Vibration Thresholds ────────────────────────────
const float VIB_THRESHOLD      = 2.5;  // m/s² above gravity
const int   VIB_COUNT_REQUIRED = 10;   // out of 20 samples

// ─── Calibration ─────────────────────────────────────────────
float mq2_bl = 0, mq4_bl = 0, mq135_bl = 0;
bool  calibrated           = false;
unsigned long calStartTime = 0, lastDailyCal = 0;
const unsigned long CAL_DURATION  = 20000UL;
const unsigned long DAILY_INTERVAL = 86400000UL;

// ─── TX Control ──────────────────────────────────────────────
unsigned long lastTxTime = 0;
bool wasDanger = false;

// ─── Live Worker Status (from received LoRa packets) ─────────
String lastWorkerZone  = "-";
String lastWorkerSOS   = "OK";

// ============================================================
//  MPU6050 — Structural Vibration Detection
//  Detects: Rockfall, blast, seismic tremor
// ============================================================
bool detectVibration() {
  int highCount = 0;
  for (int i = 0; i < 20; i++) {
    sensors_event_t a, g, t;
    mpu.getEvent(&a, &g, &t);
    float mag = sqrt(a.acceleration.x * a.acceleration.x +
                     a.acceleration.y * a.acceleration.y +
                     a.acceleration.z * a.acceleration.z);
    if (abs(mag - 9.8) > VIB_THRESHOLD) highCount++;
    delay(5);
  }
  return (highCount >= VIB_COUNT_REQUIRED);
}

// ============================================================
//  OLED Display
// ============================================================
void showOLED(float temp, float humid, int mq2, int mq4,
              int lightPct, bool danger, bool vibration,
              bool darkAlert, int workers) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Line 1: Zone
  display.setCursor(0, 0);
  display.println(String(ZONE_SSID));

  // Line 2: Temp / Humidity / Workers
  display.setCursor(0, 11);
  display.print("T:"); display.print(temp, 1); display.print("C ");
  display.print("H:"); display.print(humid, 0); display.print("% ");
  display.print("W:"); display.println(workers);

  // Line 3: Gas
  display.setCursor(0, 22);
  display.print("Gas:"); display.print(mq2);
  display.print(" CH4:"); display.print(mq4);

  // Line 4: Light % + Worker status
  display.setCursor(0, 33);
  display.print("Light:"); display.print(lightPct); display.print("%");
  display.print("  Wkr:"); display.println(lastWorkerSOS);

  // Line 5: Status bar
  display.setCursor(0, 50);
  if (darkAlert) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.println(" !! LIGHTS OUT !!    ");
  } else if (vibration) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.println(" !! VIBRATION !!     ");
  } else if (danger) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.println(" !! GAS DANGER !!    ");
  } else {
    display.println("   Status: SAFE      ");
  }
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

// ============================================================
//  Calibration
// ============================================================
bool calibrateSensors(int mq2, int mq4, int mq135) {
  // Daily recalibration
  if (calibrated && (millis() - lastDailyCal > DAILY_INTERVAL)) {
    calibrated = false;
    calStartTime = millis();
    mq2_bl = mq4_bl = mq135_bl = 0;
  }

  if (!calibrated) {
    if (millis() - calStartTime < CAL_DURATION) {
      // Running average baseline
      mq2_bl   = (mq2_bl   + mq2)   / 2.0;
      mq4_bl   = (mq4_bl   + mq4)   / 2.0;
      mq135_bl = (mq135_bl + mq135) / 2.0;

      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1);
      display.setCursor(10, 20); display.println("Calibrating...");
      display.setCursor(10, 35); display.println("Please wait 20s");
      display.display();

      LoRa.beginPacket();
      LoRa.print("{\"anchor\":\"" + ANCHOR_ID + "\",\"status\":\"calibrating\"}");
      LoRa.endPacket();
      delay(1000);
      return false;
    } else {
      calibrated = true;
      lastDailyCal = millis();
      LoRa.beginPacket();
      LoRa.print("{\"anchor\":\"" + ANCHOR_ID + "\",\"status\":\"calibrated\"}");
      LoRa.endPacket();
      Serial.println("Calibration complete.");
    }
  }
  return true;
}

// ============================================================
//  Setup
// ============================================================
void setup() {
  Serial.begin(115200);
  SPI.begin();
  Wire.begin();
  dht.begin();

  // MPU6050
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found! Check wiring.");
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);
    Serial.println("MPU6050 OK");
  }

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found! Check wiring.");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 20); display.println("MineGuard Anchor");
    display.setCursor(15, 36); display.print("Zone: ");
    display.println(ZONE_SSID);
    display.display();
    delay(2000);
  }

  // LoRa
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa failed! Check wiring.");
    while (1);
  }
  Serial.println("LoRa OK");

  // WiFi AP (Zone beacon — no internet)
  WiFi.softAP(ZONE_SSID, ZONE_PASS);
  Serial.print("WiFi AP started: ");
  Serial.println(ZONE_SSID);

  // Pins
  pinMode(MH_DO_PIN, INPUT);

  calStartTime = millis();
}

// ============================================================
//  Main Loop
// ============================================================
void loop() {
  // ── 1. Read Gas Sensors ────────────────────────────────────
  int mq2   = analogRead(MQ2_PIN);
  int mq4   = analogRead(MQ4_PIN);
  int mq135 = analogRead(MQ135_PIN);

  // ── 2. Calibration phase ───────────────────────────────────
  if (!calibrateSensors(mq2, mq4, mq135)) return;

  // ── 3. DHT11 ──────────────────────────────────────────────
  float temperature = 0.0, humidity = 0.0;
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (!isnan(event.temperature)) temperature = event.temperature;
  dht.humidity().getEvent(&event);
  if (!isnan(event.relative_humidity)) humidity = event.relative_humidity;

  // ── 4. MH Light Sensor — Ambient Light Level ─────────────────
  // LDR: low ADC value = bright light, high ADC value = dark
  // Invert and map to percentage: 100% = fully bright, 0% = total dark
  int rawLight  = analogRead(MH_AO_PIN);
  int lightPct  = map(rawLight, 4095, 0, 0, 100); // inverted mapping
  lightPct      = constrain(lightPct, 0, 100);
  bool mhDark   = (digitalRead(MH_DO_PIN) == LOW); // LOW = below threshold = dark
  bool dark_alert = (lightPct < DARK_ALERT) || mhDark;

  // ── 5. MPU6050 Vibration ──────────────────────────────────
  bool structuralVibration = detectVibration();

  // ── 6. Alert Logic ─────────────────────────────────────────
  bool gas_alert  = (mq2   > MQ2_DANGER);
  bool meth_alert = (mq4   > MQ4_DANGER);
  bool air_alert  = (mq135 > MQ135_DANGER);
  bool danger     = gas_alert || meth_alert || air_alert ||
                    structuralVibration || dark_alert;

  // ── 7. Count Workers in Zone ──────────────────────────────
  int workers = WiFi.softAPgetStationNum();

  // ── 8. Dynamic TX Interval (10 min normal / 10 sec danger) ─
  unsigned long now = millis();
  unsigned long txInterval = danger ? 10000UL : 600000UL;
  if (danger && !wasDanger) lastTxTime = 0; // Immediate TX on new danger
  wasDanger = danger;

  // ── 9. Listen for Worker LoRa Packets ────────────────────
  int pktSize = LoRa.parsePacket();
  if (pktSize) {
    String msg = "";
    while (LoRa.available()) msg += (char)LoRa.read();
    lastWorkerSOS  = (msg.indexOf("\"sos\":true")   >= 0) ? "SOS!"   : "OK";
    if (msg.indexOf("\"zone\":\"") >= 0) {
      int idx = msg.indexOf("\"zone\":\"") + 8;
      lastWorkerZone = msg.substring(idx, msg.indexOf("\"", idx));
    }
  }

  // ── 10. Serial Monitor — Formatted Debug Output ───────────
  Serial.println("─────────────────────────────────────");
  Serial.print  ("Zone       : "); Serial.println(ZONE_SSID);
  Serial.print  ("Workers    : "); Serial.println(workers);
  Serial.println("--- Environment ---");
  Serial.print  ("Temp       : "); Serial.print(temperature, 1); Serial.println(" °C");
  Serial.print  ("Humidity   : "); Serial.print(humidity, 1);    Serial.println(" %");
  Serial.println("--- Gas Sensors ---");
  Serial.print  ("MQ-2 (Gas) : "); Serial.print(mq2);   Serial.println(gas_alert  ? " ⚠ ALERT" : " OK");
  Serial.print  ("MQ-4 (CH4) : "); Serial.print(mq4);   Serial.println(meth_alert ? " ⚠ ALERT" : " OK");
  Serial.print  ("MQ-135 AQI : "); Serial.print(mq135); Serial.println(air_alert  ? " ⚠ ALERT" : " OK");
  Serial.println("--- Light ---");
  Serial.print  ("Light Level: "); Serial.print(lightPct); Serial.println(" %");
  Serial.print  ("Dark Alert : "); Serial.println(dark_alert ? "YES ⚠" : "No");
  Serial.println("--- Structure ---");
  Serial.print  ("Vibration  : "); Serial.println(structuralVibration ? "YES ⚠ DETECTED" : "No");
  Serial.println("--- Worker Status ---");
  Serial.print  ("Worker Zone: "); Serial.println(lastWorkerZone);
  Serial.print  ("Worker SOS : "); Serial.println(lastWorkerSOS);
  Serial.print  ("DANGER     : "); Serial.println(danger ? "*** YES ***" : "No");
  Serial.println("─────────────────────────────────────");


  // ── 10. Update OLED ──────────────────────────────────────
  showOLED(temperature, humidity, mq2, mq4, lightPct,
           danger, structuralVibration, dark_alert, workers);

  // ── 11. Transmit at Interval ──────────────────────────────
  if (now - lastTxTime >= txInterval || lastTxTime == 0) {
    lastTxTime = now;

    String payload = "{";
    payload += "\"type\":\"env\",";
    payload += "\"anchor\":\""   + ANCHOR_ID          + "\",";
    payload += "\"zone\":\""     + String(ZONE_SSID)   + "\",";
    payload += "\"workers\":"    + String(workers)      + ",";
    payload += "\"mq2\":"        + String(mq2)          + ",";
    payload += "\"mq4\":"        + String(mq4)          + ",";
    payload += "\"mq135\":"      + String(mq135)        + ",";
    payload += "\"temp\":"       + String(temperature,1)+ ",";
    payload += "\"humid\":"      + String(humidity,1)   + ",";
    payload += "\"light_pct\":"  + String(lightPct)     + ",";
    payload += "\"vibration\":"  + String(structuralVibration ? "true":"false") + ",";
    payload += "\"dark_alert\":" + String(dark_alert    ? "true":"false") + ",";
    payload += "\"gas_alert\":"  + String(gas_alert     ? "true":"false") + ",";
    payload += "\"meth_alert\":" + String(meth_alert    ? "true":"false") + ",";
    payload += "\"air_alert\":"  + String(air_alert     ? "true":"false") + ",";
    payload += "\"danger\":"     + String(danger        ? "true":"false");
    payload += "}";


    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket();
    Serial.println("TX: " + payload);
  }

  delay(100);
}
