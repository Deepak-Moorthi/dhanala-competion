# Worker Helmet Node — Wiring & Code Guide

---

## Components (No RFID)

| Component | Type | What It Detects / Does |
|:---|:---|:---|
| **LoRa SX1278** | Radio | Sends all helmet data to the Anchor Node |
| **MPU6050** | IMU / Accelerometer | Detects falls using freefall + impact pattern |
| **SW-420** | Shock sensor | Detects sudden physical shock or impact instantly |
| **Pulse Sensor** | Analog heart rate | Measures BPM — alerts if too low (<50) or too high (>130) |
| **DHT11** | Temp & Humidity | Measures body temperature — alerts if fever (>38.5 °C) |
| **SOS Button** | Push button | Worker manually triggers emergency SOS |
| **Buzzer (3.3V)** | Audio output | SOS morse pattern, shock beep, command acknowledgement |
| **OLED 0.96"** | Display | Shows zone, health data, direction arrows from host |

---

## ⚠️ Key Rules

> **ADC Input-Only Pins (GPIO 36, 39):** These cannot be OUTPUT. They are used for SW-420 and Pulse Sensor.
> **Buzzer:** Direct 3.3V connection — no transistor needed.
> **I2C (GPIO 21/22):** OLED and MPU6050 share the same SDA/SCL lines safely.

---

## Pin Summary

| GPIO | Connected To | Type |
|:---|:---|:---|
| 5 | LoRa NSS/CS | SPI |
| 14 | LoRa RST | Digital |
| 26 | LoRa DIO0 | Digital |
| 18 | LoRa SCK (SPI shared) | SPI |
| 19 | LoRa MISO (SPI shared) | SPI |
| 23 | LoRa MOSI (SPI shared) | SPI |
| 21 | OLED SDA + MPU6050 SDA | I2C |
| 22 | OLED SCL + MPU6050 SCL | I2C |
| 36 | SW-420 DO | Input-only |
| 39 | Pulse Sensor Signal | Input-only Analog |
| 33 | SOS Button | Digital Input |
| 13 | Buzzer (+) | Digital Output |
| 4 | DHT11 DATA | Digital |

---

## Wiring Connections

### LoRa SX1278

```
LoRa VCC   --> 3.3V
LoRa GND   --> GND
LoRa SCK   --> GPIO 18
LoRa MISO  --> GPIO 19
LoRa MOSI  --> GPIO 23
LoRa NSS   --> GPIO 5
LoRa RST   --> GPIO 14
LoRa DIO0  --> GPIO 26
```

### I2C Bus — OLED + MPU6050 (shared)

```
3.3V   --> OLED VCC + MPU6050 VCC
GND    --> OLED GND + MPU6050 GND
GPIO21 --> OLED SDA + MPU6050 SDA  (same wire)
GPIO22 --> OLED SCL + MPU6050 SCL  (same wire)
```

> OLED address: 0x3C | MPU6050 address: 0x68 — no conflict.

### SW-420 — Shock Sensor

```
3.3V   --> SW-420 VCC
GND    --> SW-420 GND
GPIO36 --> SW-420 DO
```

> Triggers HIGH when vibration/shock is detected. GPIO36 is input-only — no pull-up needed.

### Pulse Sensor

```
3.3V   --> Pulse Sensor VCC
GND    --> Pulse Sensor GND
GPIO39 --> Pulse Sensor Signal (analog)
```

> Clip to fingertip or earlobe. GPIO39 is ADC1 input-only — safe with WiFi active.

### DHT11 — Body Temperature

```
3.3V   --> DHT11 VCC
GND    --> DHT11 GND
GPIO4  --> DHT11 DATA  (+ 10kΩ pull-up from DATA to VCC)
```

### SOS Button

```
GPIO33 --> Button Pin 1
GND    --> Button Pin 2   (INPUT_PULLUP — no resistor needed)
```

### Buzzer (3.3V — Direct)

```
GPIO13 --> Buzzer (+)
GND    --> Buzzer (-)
```

### Power

```
LiPo 3.7V --> TP4056 (B+/B-)
TP4056 OUT --> ESP32 3.3V --> All components
```

---

## How Each Sensor Detects & What Happens

### 🫀 Pulse Sensor (BPM)
- Reads raw analog signal from fingertip
- Detects heartbeat peaks using rising-edge detection
- Calculates BPM from the time between peaks
- **Alert triggers if:** BPM < 50 (unconscious) or BPM > 130 (heat stroke)

### 🌡️ DHT11 (Body Temperature)
- Reads tunnel/body ambient temperature and humidity
- **Alert triggers if:** Temperature > 38.5 °C (fever/heat exhaustion)

### 🪨 SW-420 (Shock)
- Detects any sudden physical vibration or impact
- Debounced: reads 5 times in 50ms, triggers if 3+ readings are HIGH
- **Alert triggers if:** Strong shock detected (fall, rockfall on worker)

### 🔄 MPU6050 (Intelligent Fall)
- Measures acceleration in X, Y, Z axes continuously
- **Two-phase pattern:**
  - Phase 1: Total acceleration drops below 3.5 m/s² → free-fall
  - Phase 2: Acceleration spikes above 18 m/s² within 1.5s → hard landing
- Only confirms a fall if BOTH phases occur in sequence — no false alarms

### 📻 LoRa (Radio Communication)
- **Sends:** JSON payload every loop to the nearest Anchor Node
- **Receives:** Direction commands from the host (`LEFT`, `RIGHT`, `EVAC`, etc.)

### 📡 WiFi (Location)
- Scans for all `Mine_Zone_*` SSIDs broadcast by Anchor Nodes
- Measures RSSI from each anchor
- Converts RSSI to distance in metres using: `Distance = 10^((TxPower - RSSI) / (10 × n))`
- Connects to the strongest anchor = current worker zone

---

## How Data Flows Between Nodes

```
[Worker Helmet ESP32]
        │
        │  LoRa packet (JSON) — every loop cycle
        │  Contains: zone, BPM, temp, humid,
        │            SOS, shock, fall, anchor list
        ▼
[Anchor Node ESP32]  ◄── also receives WiFi connection
        │                 from helmet (counts workers in zone)
        │  LoRa packet (JSON) — every 20 sec / immediate on danger
        │  Contains: gas levels, temp, light%,
        │            vibration, workers in zone, danger flag
        ▼
[Surface Gateway ESP32]
        │
        │  HTTP POST (WiFi / internet)
        ▼
[Cloud Dashboard]
        │
        │  Direction commands back down:
        │  Dashboard → Gateway → LoRa → Anchor → Worker Helmet OLED
        ▼
[Worker sees arrow on OLED: < > ^ v or EVACUATE]
```

---

## Libraries to Install

| Library | Search In Library Manager |
|:---|:---|
| LoRa | `LoRa` by Sandeep Mistry |
| Adafruit SSD1306 | `Adafruit SSD1306` |
| Adafruit GFX Library | `Adafruit GFX Library` |
| Adafruit MPU6050 | `Adafruit MPU6050` |
| Adafruit Unified Sensor | `Adafruit Unified Sensor` |
| DHT sensor library | `DHT sensor library` by Adafruit |

---

## JSON Payload Sent by Helmet

```json
{
  "type":         "worker",
  "id":           "W-01",
  "zone":         "Mine_Zone_2",
  "bpm":          72,
  "temp":         36.8,
  "humid":        55.0,
  "health_alert": false,
  "sos":          false,
  "shock":        false,
  "fall":         false,
  "anchors": [
    {"ssid": "Mine_Zone_1", "rssi": -68, "dist": 6.2},
    {"ssid": "Mine_Zone_2", "rssi": -45, "dist": 1.9}
  ]
}
```
