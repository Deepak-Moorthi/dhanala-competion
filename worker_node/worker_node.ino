// ============================================================
//  WORKER HELMET NODE — Complete Firmware
//  Sensors: LoRa, WiFi-STA, OLED, SW-420, MPU6050,
//           Pulse Sensor, DHT11, SOS Button, Buzzer
//  No RFID.
// ============================================================

#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Adafruit_MPU6050.h>
#include <math.h>

// ─── LoRa ─────────────────────────────────────────────────────
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 26

// ─── Sensors & Actuators ─────────────────────────────────────
#define SW420_PIN  36   // SW-420 shock sensor (ADC1 input-only)
#define PULSE_PIN  39   // Pulse sensor analog  (ADC1 input-only)
#define SOS_BTN    33   // SOS push button — Active LOW
#define BUZZER_PIN 13   // 3.3V buzzer — direct GPIO

// ─── DHT11 ────────────────────────────────────────────────────
#define DHTPIN  4
#define DHTTYPE DHT11
DHT_Unified dht(DHTPIN, DHTTYPE);

// ─── OLED + MPU6050 (shared I2C GPIO21/22) ───────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_MPU6050 mpu;

// ─── Config ───────────────────────────────────────────────────
const String WORKER_ID   = "W-01";       // ← Change per helmet
const String ZONE_PREFIX = "Mine_Zone_";
const char*  ZONE_PASS   = "safety123";

// ─── AI Distance (RSSI → Metres) ─────────────────────────────
const float TX_POWER    = -40.0;
const float PATH_LOSS_N =  3.5;

// ─── Health Thresholds ────────────────────────────────────────
const int   BPM_LOW   = 50;
const int   BPM_HIGH  = 130;
const float TEMP_HIGH = 38.5;

// ─── Fall Detection Thresholds ────────────────────────────────
const float FREEFALL_THRESHOLD = 3.5;
const float IMPACT_THRESHOLD   = 18.0;

// ─── AnchorReading — MUST be global before any function uses it
struct AnchorReading {
  String ssid;
  int    rssi;
  float  dist_m;
};

// ─── State ────────────────────────────────────────────────────
String currentConnectedZone = "Unknown";
float  bodyTemp  = 0.0;
float  bodyHumid = 0.0;
int    bpm = 0;
int    lastPulseRaw = 0;
unsigned long lastBeatTime = 0;

// ════════════════════════════════════════════════════════════
//  BPM — Peak Detection on analog pulse sensor
// ════════════════════════════════════════════════════════════
int readBPM() {
  int raw = analogRead(PULSE_PIN);
  unsigned long now = millis();
  if (raw > 2000 && lastPulseRaw <= 2000) {
    if (lastBeatTime > 0) {
      unsigned long interval = now - lastBeatTime;
      if (interval > 300 && interval < 1500) bpm = 60000 / interval;
    }
    lastBeatTime = now;
  }
  lastPulseRaw = raw;
  if (now - lastBeatTime > 3000) bpm = 0;
  return bpm;
}

// ════════════════════════════════════════════════════════════
//  SW-420 — Debounced shock detection
// ════════════════════════════════════════════════════════════
bool detectSW420Shock() {
  int hit = 0;
  for (int i = 0; i < 5; i++) {
    if (digitalRead(SW420_PIN) == HIGH) hit++;
    delay(10);
  }
  return (hit >= 3);
}

// ════════════════════════════════════════════════════════════
//  MPU6050 — Intelligent Fall Detection
//  Phase 1: freefall (acceleration < 3.5 m/s²)
//  Phase 2: hard impact (acceleration > 18 m/s²) within 1.5s
// ════════════════════════════════════════════════════════════
bool detectMPUFall() {
  sensors_event_t a, g, temp;
  if (!mpu.getEvent(&a, &g, &temp)) return false;
  float mag = sqrt(a.acceleration.x * a.acceleration.x +
                   a.acceleration.y * a.acceleration.y +
                   a.acceleration.z * a.acceleration.z);
  static bool freefallDetected = false;
  static unsigned long freefallTime = 0;
  if (mag < FREEFALL_THRESHOLD) {
    freefallDetected = true;
    freefallTime = millis();
  }
  if (freefallDetected && mag > IMPACT_THRESHOLD) {
    if (millis() - freefallTime < 1500) { freefallDetected = false; return true; }
    freefallDetected = false;
  }
  if (freefallDetected && millis() - freefallTime > 2000) freefallDetected = false;
  return false;
}

// ════════════════════════════════════════════════════════════
//  AI Distance + WiFi Zone Scan
// ════════════════════════════════════════════════════════════
float rssiToDistance(int rssi) {
  return pow(10.0, (TX_POWER - rssi) / (10.0 * PATH_LOSS_N));
}

String connectToStrongestZone(AnchorReading* readings, int &count) {
  int n = WiFi.scanNetworks();
  count = 0;
  String bestZone = "Unknown";
  int bestRSSI = -1000;
  for (int i = 0; i < n && count < 5; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.startsWith(ZONE_PREFIX)) {
      int rssi = WiFi.RSSI(i);
      readings[count] = { ssid, rssi, rssiToDistance(rssi) };
      count++;
      if (rssi > bestRSSI) { bestRSSI = rssi; bestZone = ssid; }
    }
  }
  WiFi.scanDelete();
  if (bestZone != "Unknown" && bestZone != currentConnectedZone) {
    WiFi.begin(bestZone.c_str(), ZONE_PASS);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries++ < 6) delay(500);
    if (WiFi.status() == WL_CONNECTED) currentConnectedZone = bestZone;
  }
  return currentConnectedZone;
}

// ════════════════════════════════════════════════════════════
//  OLED Screens
// ════════════════════════════════════════════════════════════
void showStatus(String zone, bool sos, bool fall, bool shock) {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0, 0);  display.print("MineGuard W:"); display.println(WORKER_ID);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  display.setCursor(0, 13); display.print("Zone: "); display.println(zone);
  display.setCursor(0, 33);
  if (sos)        display.println("!! SOS ACTIVATED !!");
  else if (fall)  display.println("!! FALL DETECTED !!");
  else if (shock) display.println("!! SHOCK DETECTED !!");
  else            display.println("Status: OK");
  display.display();
}

void showHealth(int bpmVal, float temp, float humid, bool alert) {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0, 0); display.println("-- Health Monitor --");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  display.setTextSize(2); display.setCursor(0, 14);
  if (bpmVal > 0) { display.print(bpmVal); display.setTextSize(1); display.print(" BPM"); }
  else { display.setTextSize(1); display.println("No BPM signal"); }
  display.setTextSize(1);
  display.setCursor(0, 36); display.print("Temp : "); display.print(temp, 1); display.println(" C");
  display.setCursor(0, 46); display.print("Humid: "); display.print(humid, 0); display.println(" %");
  if (alert) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setCursor(0, 56); display.println("  HEALTH ALERT!  ");
    display.setTextColor(SSD1306_WHITE);
  }
  display.display();
}

void showDirection(String dir, String msg) {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(3);
  display.setCursor(45, 8);
  if      (dir == "LEFT")    display.println("<");
  else if (dir == "RIGHT")   display.println(">");
  else if (dir == "FORWARD") display.println("^");
  else if (dir == "BACK")    display.println("v");
  else if (dir == "EVAC")    { display.setTextSize(2); display.setCursor(10, 8); display.println("EVACUATE"); }
  display.setTextSize(1); display.setCursor(0, 50); display.println(msg);
  display.display();
}

void showDanger(String msg) {
  display.clearDisplay();
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.fillRect(0, 0, 128, 16, SSD1306_WHITE);
  display.setTextSize(1); display.setCursor(20, 4); display.println("!! DANGER !!");
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 22); display.println(msg);
  display.display();
}

// ════════════════════════════════════════════════════════════
//  Buzzer Patterns
// ════════════════════════════════════════════════════════════
void buzzerSOS() {
  for (int i = 0; i < 3; i++) { digitalWrite(BUZZER_PIN, HIGH); delay(150); digitalWrite(BUZZER_PIN, LOW); delay(150); }
  for (int i = 0; i < 3; i++) { digitalWrite(BUZZER_PIN, HIGH); delay(400); digitalWrite(BUZZER_PIN, LOW); delay(150); }
  for (int i = 0; i < 3; i++) { digitalWrite(BUZZER_PIN, HIGH); delay(150); digitalWrite(BUZZER_PIN, LOW); delay(150); }
}
void buzzerAlert() { digitalWrite(BUZZER_PIN, HIGH); delay(2000); digitalWrite(BUZZER_PIN, LOW); }
void buzzerAck()   { for (int i = 0; i < 2; i++) { digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW); delay(100); } }

// ════════════════════════════════════════════════════════════
//  Setup
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  SPI.begin();
  Wire.begin();
  dht.begin();

  if (!mpu.begin()) {
    Serial.println("MPU6050 not found! Check wiring.");
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.println("MPU6050 OK");
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found! Check wiring.");
  } else {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
    display.setCursor(15, 20); display.println("MineGuard Helmet");
    display.setCursor(20, 36); display.println("ID: " + WORKER_ID);
    display.display(); delay(2000);
  }

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) { Serial.println("LoRa failed! Check wiring."); while (1); }
  Serial.println("LoRa OK");

  pinMode(SW420_PIN,  INPUT);
  pinMode(SOS_BTN,    INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  Serial.println("Worker Helmet Ready. ID: " + WORKER_ID);
}

// ════════════════════════════════════════════════════════════
//  Loop
// ════════════════════════════════════════════════════════════
void loop() {
  // 1. SOS Button
  bool sos = (digitalRead(SOS_BTN) == LOW);
  if (sos) { buzzerSOS(); showDanger("SOS sent to host!"); }

  // 2. Dual fall detection
  bool shock = detectSW420Shock();
  bool fall  = detectMPUFall();
  if (fall)       { buzzerSOS();   showDanger("FALL DETECTED!"); }
  else if (shock) { buzzerAlert(); showDanger("Shock / impact!"); }

  // 3. Pulse sensor BPM
  int currentBPM = readBPM();

  // 4. DHT11 body readings
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (!isnan(event.temperature)) bodyTemp = event.temperature;
  dht.humidity().getEvent(&event);
  if (!isnan(event.relative_humidity)) bodyHumid = event.relative_humidity;

  // 5. Health alert
  bool healthAlert = (currentBPM > 0 && (currentBPM < BPM_LOW || currentBPM > BPM_HIGH)) ||
                     (bodyTemp > TEMP_HIGH);
  if (healthAlert) buzzerAlert();

  // 6. WiFi Zone Scan (every 15 seconds)
  static unsigned long lastScan     = 0;
  static unsigned long lastOLEDSwap = 0;
  static bool showHealthScreen      = false;
  AnchorReading readings[5];
  int anchorCount = 0;
  if (millis() - lastScan > 15000 || lastScan == 0) {
    connectToStrongestZone(readings, anchorCount);
    lastScan = millis();
  }

  // 7. OLED — toggle status/health every 5s
  if (millis() - lastOLEDSwap > 5000) { showHealthScreen = !showHealthScreen; lastOLEDSwap = millis(); }
  if (!sos && !shock && !fall && !healthAlert) {
    if (showHealthScreen) showHealth(currentBPM, bodyTemp, bodyHumid, false);
    else                  showStatus(currentConnectedZone, false, false, false);
  } else if (healthAlert)  showHealth(currentBPM, bodyTemp, bodyHumid, true);

  // 8. Serial Monitor
  Serial.println("=====================================");
  Serial.print("Worker ID  : "); Serial.println(WORKER_ID);
  Serial.print("Zone       : "); Serial.println(currentConnectedZone);
  Serial.println("--- Health ---");
  Serial.print("BPM        : "); Serial.println(currentBPM);
  Serial.print("Body Temp  : "); Serial.print(bodyTemp, 1);  Serial.println(" C");
  Serial.print("Humidity   : "); Serial.print(bodyHumid, 0); Serial.println(" %");
  Serial.print("Hlth Alert : "); Serial.println(healthAlert ? "YES!" : "No");
  Serial.println("--- Safety ---");
  Serial.print("SOS        : "); Serial.println(sos   ? "YES!" : "No");
  Serial.print("Shock(SW420): "); Serial.println(shock ? "YES!" : "No");
  Serial.print("Fall (MPU) : "); Serial.println(fall  ? "YES!" : "No");
  Serial.println("--- Anchors Visible ---");
  for (int i = 0; i < anchorCount; i++) {
    Serial.print("  "); Serial.print(readings[i].ssid);
    Serial.print("  RSSI:"); Serial.print(readings[i].rssi);
    Serial.print("  Dist:"); Serial.print(readings[i].dist_m, 1); Serial.println("m");
  }
  Serial.println("=====================================");

  // 9. LoRa Payload
  String p = "{";
  p += "\"type\":\"worker\",";
  p += "\"id\":\""   + WORKER_ID            + "\",";
  p += "\"zone\":\"" + currentConnectedZone  + "\",";
  p += "\"bpm\":"    + String(currentBPM)   + ",";
  p += "\"temp\":"   + String(bodyTemp, 1)  + ",";
  p += "\"humid\":"  + String(bodyHumid, 1) + ",";
  p += "\"health_alert\":" + String(healthAlert ? "true" : "false") + ",";
  p += "\"sos\":"    + String(sos   ? "true" : "false") + ",";
  p += "\"shock\":"  + String(shock ? "true" : "false") + ",";
  p += "\"fall\":"   + String(fall  ? "true" : "false") + ",";
  p += "\"anchors\":[";
  for (int i = 0; i < anchorCount; i++) {
    if (i > 0) p += ",";
    p += "{\"ssid\":\"" + readings[i].ssid              + "\",";
    p += "\"rssi\":"    + String(readings[i].rssi)      + ",";
    p += "\"dist\":"    + String(readings[i].dist_m, 1) + "}";
  }
  p += "]}";
  Serial.println("TX: " + p);
  LoRa.beginPacket(); LoRa.print(p); LoRa.endPacket();

  // 10. Receive direction commands from host
  int pktSize = LoRa.parsePacket();
  if (pktSize) {
    String cmd = "";
    while (LoRa.available()) cmd += (char)LoRa.read();
    Serial.println("CMD: " + cmd);
    if      (cmd.indexOf("LEFT")    >= 0) { showDirection("LEFT",    "Turn LEFT now");  buzzerAck(); }
    else if (cmd.indexOf("RIGHT")   >= 0) { showDirection("RIGHT",   "Turn RIGHT now"); buzzerAck(); }
    else if (cmd.indexOf("FORWARD") >= 0) { showDirection("FORWARD", "Move FORWARD");   buzzerAck(); }
    else if (cmd.indexOf("BACK")    >= 0) { showDirection("BACK",    "Go BACK");        buzzerAck(); }
    else if (cmd.indexOf("EVAC")    >= 0) { showDirection("EVAC",    "Leave mine NOW!"); buzzerSOS(); }
    else if (cmd.indexOf("ALERT")   >= 0) { showDanger(cmd); buzzerAlert(); }
  }

  delay(100);
}
