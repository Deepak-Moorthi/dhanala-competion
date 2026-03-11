# Underground Worker Safety System - Startup Guide

This project consists of three main parts: 
1. **Worker Helmet Node (Arduino)**
2. **Surface Gateway (Arduino)**
3. **Cloud Dashboard (Node.js)**

## 1. Hardware Setup
- **Worker Node:** Connect your MQ sensors, DHT11, MPU6050, LoRa, and Vibration motor to the ESP32 as defined in `worker_node.ino`.
- **Gateway:** Connect the LoRa module to a second ESP32.
- **WiFi Beacons:** Use any standard WiFi routers. Set their SSIDs to start with `Mine_Zone_` (e.g., `Mine_Zone_1`, `Mine_Zone_2`) along the tunnel path.

## 2. Firmware Installation
1. Open the [worker_node.ino](worker_node/worker_node.ino) and [surface_gateway.ino](surface_gateway/surface_gateway.ino) in the Arduino IDE.
2. Install the following libraries via the Library Manager:
   - `LoRa` by Sandeep Mistry
   - `Adafruit Unified Sensor`
   - `DHT sensor library`
   - `Adafruit MPU6050`
3. In `surface_gateway.ino`, update `WIFI_SSID`, `WIFI_PASS`, and `CLOUD_SERVER_URL` with your details.
4. Upload both sketches to your respective ESP32 boards.

## 3. Running the Dashboard
The dashboard allows you to see the real-time location and vitals of workers online.

1. Open a terminal in the `./dashboard` folder.
2. Run the following commands to install dependencies:
   ```bash
   npm install express socket.io body-parser
   ```
3. Start the server:
   ```bash
   node server.js
   ```
4. Open your browser to `http://localhost:3000`.

## 4. Testing without a Cloud Server
If you are testing locally at the hackathon:
1. Connect your Surface Gateway ESP32 to your laptop.
2. Use a tool like **Ngrok** to expose your `localhost:3000` to the internet.
3. Use the Ngrok URL (e.g., `https://random-id.ngrok-free.app/api/telemetry`) as the `CLOUD_SERVER_URL` in your `surface_gateway.ino`.

---
**Hackathon Tip:** If you don't have enough routers, you can use extra ESP32s or even your Smartphones to create Mobile Hotspots with names like `Mine_Zone_1` to test the location tracking!
