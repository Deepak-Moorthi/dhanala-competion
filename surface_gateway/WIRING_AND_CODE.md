# Surface Gateway Node — Wiring & Code Guide

---

## What This Node Does

The Gateway sits at the **tunnel entrance** where it has access to real internet WiFi.  
It listens for incoming LoRa radio packets from the Helmet Nodes underground,  
then forwards them to your Cloud Dashboard via an HTTP POST request.

---

## Wiring Connections

### SPI Bus (LoRa Only)

```
ESP32           LoRa SX1278
-------         -----------
3.3V    ------> VCC
GND     ------> GND
GPIO18  ------> SCK
GPIO19  ------> MISO
GPIO23  ------> MOSI
GPIO5   ------> NSS / CS
GPIO14  ------> RST
GPIO26  ------> DIO0
```

### Power

```
USB Cable --> ESP32 microUSB
(Connect to laptop or wall adapter at the surface)
```

No other components are needed for the Gateway.

---

## Configuration Before Flashing

Open `surface_gateway/surface_gateway.ino` and fill in these values:

```cpp
const char* WIFI_SSID = "YOUR_INTERNET_WIFI_NAME";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char* CLOUD_SERVER_URL = "http://your-dashboard-url.com/api/telemetry";
```

> For local testing, start the dashboard first (`node server.js`), then use  
> ngrok to get a public URL: `ngrok http 3000`  
> Use the ngrok URL as your `CLOUD_SERVER_URL`.

---

## Code File

→ Open: `surface_gateway/surface_gateway.ino`

### Key Behaviours

| Feature | What it does |
|:---|:---|
| LoRa Listener | Continuously listens for incoming radio packets |
| HTTP POST | Forwards received JSON payload to Cloud Dashboard |
| Two-Way Bridge | If Dashboard sends back a command (e.g., "EVAC"), rebroadcasts via LoRa to the Helmet |

---

## Verification

After flashing, open Serial Monitor (115200 baud). You should see:

```
Connecting to Wi-Fi: YourWiFiName
..........
Wi-Fi Connected!
IP Address: 192.168.x.x
LoRa Initialized (Listening on 433MHz).
Received LoRa Packet: {"id":"W-01","z":"Mine_Zone_1",...}
HTTP Response Code: 200
```
