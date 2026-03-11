# Cloud Dashboard — Setup & Code Guide

---

## What This Does

The Dashboard is a Node.js web application that:
- Receives live JSON telemetry from the Surface Gateway via HTTP POST on port **3001**
- Displays a real-time tunnel map with worker positions
- Shows health cards with BPM, temperature, gas levels per worker
- Lets the operator send evacuation/direction commands back into the tunnel

---

## Files

```
dashboard/
├── server.js          ← Node.js backend (HTTP + Socket.io)
└── public/
    └── index.html     ← Frontend live map dashboard
```

---

## Step 1 — Flash the Surface Gateway ESP32

Open `surface_gateway/surface_gateway.ino` in Arduino IDE.

**Change these 3 lines:**

```cpp
// Line 1 — Your WiFi name (the router the gateway connects to)
const char *WIFI_SSID = "YOUR_WIFI_NAME";

// Line 2 — Your WiFi password
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Line 3 — Dashboard server IP (your PC's IP on the same network)
const char *CLOUD_SERVER_URL = "http://192.168.2.63:3001/api/telemetry";
```

**Upload to ESP32:**
1. Connect ESP32 via USB
2. **Tools → Board → ESP32 Dev Module**
3. **Tools → Port → your COM port** (COM3, COM5, etc.)
4. Click **Upload ▶**
5. **Tools → Serial Monitor → 115200 baud**

**Expected Serial Monitor output:**
```
Connecting to Wi-Fi: YourWiFiName
....
Wi-Fi Connected!
IP Address: 192.168.2.XX
LoRa Initialized (Listening on 433MHz).
Received LoRa Packet: {"type":"env","anchor":"A-01",...}
Forwarding to Cloud: http://192.168.2.63:3001/api/telemetry
HTTP Response Code: 200   ← 200 = SUCCESS ✅
```

---

## Step 2 — Start the Dashboard on Your PC

Open a terminal in the `dashboard/` folder:

```bash
# First time only — install dependencies
npm install express socket.io body-parser

# Start the server
node server.js
```

Open your browser:
```
http://localhost:3001
```

**Backend console will show:**
```
[HTTP Telemetry] ✅ A-01 → Gas:430 Temp:28.5 Light:72% Danger:false
[HTTP Telemetry] ✅ W-01 → BPM:72 Temp:36.8 Zone:Mine_Zone_2 Fall:false
```

---

## Step 3 — Allow Port 3001 in Windows Firewall (Do Once)

The ESP32 sends from the network to your PC. Windows may block it:

1. Press **Windows Key** → search **"Windows Defender Firewall"**
2. Click **"Advanced Settings"** (left panel)
3. **"Inbound Rules"** → **"New Rule..."**
4. Select **Port** → Next → **TCP** → type `3001` → Next
5. **"Allow the connection"** → Next → Next → Name it `MineGuard` → Finish

---

## Step 4 — Quick Test (No ESP32 Needed)

Paste this in Command Prompt to simulate an anchor sending data:

```bash
curl -X POST http://localhost:3001/api/telemetry ^
  -H "Content-Type: application/json" ^
  -d "{\"type\":\"env\",\"anchor\":\"A-01\",\"zone\":\"Mine_Zone_1\",\"workers\":2,\"mq2\":430,\"mq4\":310,\"mq135\":580,\"temp\":28.5,\"humid\":65.0,\"light_pct\":72,\"vibration\":false,\"dark_alert\":false,\"gas_alert\":false,\"danger\":false}"
```

To simulate a worker helmet:
```bash
curl -X POST http://localhost:3001/api/telemetry ^
  -H "Content-Type: application/json" ^
  -d "{\"type\":\"worker\",\"id\":\"W-01\",\"zone\":\"Mine_Zone_2\",\"bpm\":72,\"temp\":36.8,\"humid\":55.0,\"health_alert\":false,\"sos\":false,\"shock\":false,\"fall\":false}"
```

If server shows `200 OK` → everything is working ✅

---

## API Endpoints

| Endpoint | Method | Purpose |
|:---|:---|:---|
| `/api/telemetry` | POST | Gateway sends sensor JSON here |
| `/api/command` | POST | Operator sends `LEFT`, `RIGHT`, `EVAC` |
| `/` | GET | Serves the live HTML dashboard |

---

## Network Setup Summary

```
[Anchor ESP32] ──LoRa──▶ [Surface Gateway ESP32]
                               │  WiFi
                               ▼
                     [Your PC — 192.168.2.63]
                     [Dashboard on port 3001]
                               │  Browser
                               ▼
                     http://localhost:3001
```

> ⚠️ Make sure the Gateway ESP32 and your PC are **on the same WiFi network** for `192.168.2.63:3001` to be reachable.

---

## JSON Format Expected by Dashboard

### From Anchor Node
```json
{
  "type": "env", "anchor": "A-01", "zone": "Mine_Zone_1",
  "workers": 2, "mq2": 430, "mq4": 310, "mq135": 580,
  "temp": 28.5, "humid": 65.0, "light_pct": 72,
  "vibration": false, "dark_alert": false,
  "gas_alert": false, "danger": false
}
```

### From Worker Helmet
```json
{
  "type": "worker", "id": "W-01", "zone": "Mine_Zone_2",
  "bpm": 72, "temp": 36.8, "humid": 55.0,
  "health_alert": false, "sos": false,
  "shock": false, "fall": false,
  "anchors": [
    {"ssid": "Mine_Zone_2", "rssi": -45, "dist": 1.9}
  ]
}
```
