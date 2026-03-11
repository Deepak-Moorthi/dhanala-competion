# Worker Helmet Node — Wiring & Code Guide

---

## Sensors & Components

| Component | Type | Purpose |
|:---|:---|:---|
| LoRa SX1278 | Output / Radio | Send telemetry to host |
| MFRC522 | Input / RFID | Worker identification |
| SW-420 | Input / Digital | Instant shock / impact detection |
| **MPU6050** | **Input / I2C IMU** | **Intelligent fall pattern detection** |
| **Pulse Sensor** | **Input / Analog** | **Heart rate (BPM) monitoring** |
| **DHT11** | **Input / Digital** | **Body temperature & humidity** |
| Buzzer (3.3V) | Output | Alert sounds |
| Push Button | Input | Manual SOS trigger |
| OLED 0.96" | Output / Display | Direction arrows + health screen |

---

## Wiring Connections

### SPI Bus (LoRa + RFID — Shared)

```
ESP32           LoRa SX1278         RFID MFRC522
-------         -----------         ------------
3.3V    ------> VCC                 VCC
GND     ------> GND                 GND
GPIO18  ------> SCK    -----------> SCK
GPIO19  ------> MISO   -----------> MISO
GPIO23  ------> MOSI   -----------> MOSI
GPIO5   ------> NSS/CS
GPIO27  ----------------------------SDA/CS  ← UNIQUE CS!
GPIO14  ------> RST
GPIO2   ----------------------------RST
GPIO26  ------> DIO0
```

### I2C Bus (OLED + MPU6050 — Shared)

```
3.3V    ------> OLED VCC
3.3V    ------> MPU6050 VCC
GND     ------> OLED GND
GND     ------> MPU6050 GND
GPIO21  ------> OLED SDA + MPU6050 SDA  (same wire)
GPIO22  ------> OLED SCL + MPU6050 SCL  (same wire)
```

> Both share I2C safely — different addresses: OLED=0x3C, MPU6050=0x68

### SW-420 — Shock Sensor

```
3.3V    ------> SW-420 VCC
GND     ------> SW-420 GND
GPIO36  ------> SW-420 DO  (input-only pin)
```

### Pulse Sensor (Analog)

```
3.3V    ------> Pulse Sensor VCC
GND     ------> Pulse Sensor GND
GPIO39  ------> Pulse Sensor SIGNAL  (input-only pin)
```

> Clip to worker's fingertip or earlobe.

### DHT11 — Body Temperature & Humidity

```
3.3V    ------> DHT11 VCC
GND     ------> DHT11 GND
GPIO4   ------> DHT11 DATA  (+ 10kΩ pull-up between VCC and DATA)
```

### SOS Button

```
GPIO33  ------> Button Pin 1
GND     ------> Button Pin 2  (INPUT_PULLUP)
```

### Buzzer (3.3V — Direct)

```
GPIO13  ------> Buzzer (+)
GND     ------> Buzzer (-)
```

### Power

```
LiPo 3.7V  --> TP4056 (B+/B-)
TP4056 OUT --> ESP32 3.3V pin --> All components
```

> Everything runs on 3.3V — no boost converter needed.

---

## Libraries to Install

| Library | Author |
|:---|:---|
| LoRa | Sandeep Mistry |
| MFRC522 | GithubCommunity |
| DHT sensor library | Adafruit |
| Adafruit Unified Sensor | Adafruit |
| Adafruit MPU6050 | Adafruit |
| Adafruit SSD1306 | Adafruit |
| Adafruit GFX Library | Adafruit |

---

## Code File

→ Open: `worker_node/worker_node.ino`

### Dual Fall Detection

| Sensor | Method | Detects |
|:---|:---|:---|
| SW-420 | `digitalRead()` | Any physical shock / vibration instantly |
| MPU6050 | Freefall + Impact pattern | Confirmed fall (freefall → hard landing) |

### Health Alert Thresholds

| Condition | Threshold |
|:---|:---|
| Low BPM | < 50 BPM |
| High BPM | > 130 BPM |
| High body temp | > 38.5 °C |

### JSON Payload

```json
{
  "type": "worker", "id": "W-01", "tag": "a4f23c",
  "zone": "Mine_Zone_2",
  "bpm": 72, "temp": 36.8, "humid": 55.0,
  "health_alert": false, "sos": false,
  "shock": false, "fall": false,
  "anchors": [{"ssid":"Mine_Zone_1","rssi":-60,"dist":4.2}]
}
```
