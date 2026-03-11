# Anchor Node — Complete Wiring & Code Guide

---

## Components List

| Component | Quantity |
|:---|:---|
| ESP32 (38-pin) | 1 |
| LoRa SX1278 (433 MHz) | 1 |
| OLED 0.96" I2C (SSD1306) | 1 |
| MPU6050 (I2C IMU) | 1 |
| DHT11 | 1 |
| MQ-2 Gas Sensor | 1 |
| MQ-4 Gas Sensor | 1 |
| MQ-135 Gas Sensor | 1 |
| MH LDR Light Sensor | 1 |
| 10kΩ Resistor | 1 (for DHT11) |
| USB 5V Power / Power Bank | 1 |

---

## ⚠️ ESP32 ADC Rule

> **ADC2 pins (GPIO 25, 26, 27) DO NOT work for `analogRead()` when WiFi is active.**
> Use only **ADC1 pins: GPIO 32, 33, 34, 35, 36, 39** for all analog sensors.

---

## Complete Pin Map

| GPIO | Sensor / Module | Connection |
|:---|:---|:---|
| 5 | LoRa SX1278 | NSS / CS |
| 14 | LoRa SX1278 | RST |
| 26 | LoRa SX1278 | DIO0 |
| 18 | LoRa SX1278 | SCK (SPI) |
| 19 | LoRa SX1278 | MISO (SPI) |
| 23 | LoRa SX1278 | MOSI (SPI) |
| 21 | OLED + MPU6050 | SDA (I2C) |
| 22 | OLED + MPU6050 | SCL (I2C) |
| 32 | MQ-2 | AO (Analog Out) |
| 33 | MH Light Sensor | AO (Analog Out) |
| 34 | MQ-4 | AO (Analog Out) |
| 35 | MQ-135 | AO (Analog Out) |
| 15 | MH Light Sensor | DO (Digital Out) |
| 4 | DHT11 | DATA |

---

## Wiring Diagrams

### 1. LoRa SX1278

```
LoRa VCC    --> 3.3V
LoRa GND    --> GND
LoRa SCK    --> GPIO 18
LoRa MISO   --> GPIO 19
LoRa MOSI   --> GPIO 23
LoRa NSS/CS --> GPIO 5
LoRa RST    --> GPIO 14
LoRa DIO0   --> GPIO 26
```

### 2. OLED + MPU6050 (Shared I2C Bus)

```
OLED VCC    --> 3.3V
OLED GND    --> GND
OLED SDA    --> GPIO 21  ──┐  same wire
OLED SCL    --> GPIO 22  ──┤
                            │
MPU6050 VCC --> 3.3V        │
MPU6050 GND --> GND         │
MPU6050 SDA --> GPIO 21  ───┘
MPU6050 SCL --> GPIO 22  ───┘
```

> I2C addresses: OLED = **0x3C**, MPU6050 = **0x68** — no conflict.

### 3. Gas Sensors (MQ-2, MQ-4, MQ-135)

```
MQ-2 VCC    --> 5V        MQ-4 VCC    --> 5V        MQ-135 VCC  --> 5V
MQ-2 GND    --> GND       MQ-4 GND    --> GND       MQ-135 GND  --> GND
MQ-2 AO     --> GPIO 32   MQ-4 AO     --> GPIO 34   MQ-135 AO   --> GPIO 35
MQ-2 DO     --> (not used)
```

> MQ sensors need 5V to heat the sensing element. AO pin gives ADC1-compatible analog output.

### 4. DHT11

```
DHT11 VCC   --> 3.3V
DHT11 GND   --> GND
DHT11 DATA  --> GPIO 4
              (also connect 10kΩ resistor between VCC and DATA pin)
```

### 5. MH LDR Light Sensor (4 pins: VCC, GND, DO, AO)

```
MH VCC      --> 3.3V
MH GND      --> GND
MH AO       --> GPIO 33   (Analog: raw light level → mapped to 0–100%)
MH DO       --> GPIO 15   (Digital: LOW when darker than pot threshold)
```

> Adjust the blue potentiometer on the MH module to set the darkness threshold for the DO pin.
> AO gives precise continuous light level. DO gives a digital on/off threshold trigger.

### 6. Power

```
5V USB / Power Bank --> ESP32 VIN
GND                 --> ESP32 GND
```

> The ESP32 3.3V regulator powers: OLED, MPU6050, DHT11, MH sensor.
> MQ sensors need direct 5V from the power bank.

---

## Libraries to Install (Arduino IDE)

Search and install from **Sketch → Include Library → Manage Libraries**:

| Library Name | Author |
|:---|:---|
| LoRa | Sandeep Mistry |
| DHT sensor library | Adafruit |
| Adafruit Unified Sensor | Adafruit |
| Adafruit MPU6050 | Adafruit |
| Adafruit SSD1306 | Adafruit |
| Adafruit GFX Library | Adafruit |

---

## Configuration (Change Before Flashing Each Anchor)

```cpp
const char*  ZONE_SSID = "Mine_Zone_1"; // Change to Mine_Zone_2, _3 etc.
const String ANCHOR_ID = "A-01";         // Change to A-02, A-03 etc.
```

---

## JSON Payload Sent via LoRa

```json
{
  "type":        "env",
  "anchor":      "A-01",
  "zone":        "Mine_Zone_1",
  "workers":     2,
  "mq2":         430,
  "mq4":         310,
  "mq135":       580,
  "temp":        28.5,
  "humid":       65.0,
  "light_pct":   72,
  "vibration":   false,
  "dark_alert":  false,
  "gas_alert":   false,
  "meth_alert":  false,
  "air_alert":   false,
  "danger":      false
}
```

## TX Interval

| Condition | LoRa Send Rate |
|:---|:---|
| All normal | Every **10 minutes** |
| Any danger triggered | Every **10 seconds** |
