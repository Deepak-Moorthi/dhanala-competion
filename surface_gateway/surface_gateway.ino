#include <HTTPClient.h>
#include <LoRa.h>
#include <SPI.h>
#include <WiFi.h>


// --- LoRa Pin Definitions (Matches Hardware Document) ---
#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 26

// --- Surface Internet Connection ---
// The Gateway must be in range of a standard Wi-Fi router that has internet
// access.
const char *WIFI_SSID = "YOUR_WIFI_NAME";       // ← Put your WiFi name here
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";   // ← Put your WiFi password here

// --- Cloud Dashboard Endpoint ---
// Replace this with your actual Render/Vercel/Ngrok backend URL or a test site
// like webhook.site
const char *CLOUD_SERVER_URL = "http://192.168.2.63:3001/api/telemetry";

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;

  Serial.println("Initializing Surface Gateway Node...");

  // Setup LoRa
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed! Check wiring.");
    while (1)
      ;
  }
  Serial.println("LoRa Initialized (Listening on 433MHz).");

  // Connect to Surface Wi-Fi
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // 1. Listen for offline LoRa packets coming from deep in the tunnel
  int packetSize = LoRa.parsePacket();

  if (packetSize) {
    String loraPacket = "";
    while (LoRa.available()) {
      loraPacket += (char)LoRa.read();
    }

    Serial.print("Received LoRa Packet: ");
    Serial.println(loraPacket);

    // 2. We received Data. Let's forward it to the Cloud.
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;

      Serial.print("Forwarding to Cloud: ");
      Serial.println(CLOUD_SERVER_URL);

      http.begin(CLOUD_SERVER_URL);

      // Specify content-type as JSON
      http.addHeader("Content-Type", "application/json");

      // Send the POST request with the LoRa string payload
      int httpResponseCode = http.POST(loraPacket);

      if (httpResponseCode > 0) {
        Serial.print("HTTP Response Code: ");
        Serial.println(httpResponseCode);

        // Example: If cloud server wants to trigger an evacuation, it replies
        // with "EVACUATE_LEFT"
        String response = http.getString();

        if (response.length() > 0 && response != "OK") {
          Serial.println("Cloud sent command back: " + response);

          // Broadcast command back down the tunnel via LoRa
          LoRa.beginPacket();
          LoRa.print(response);
          LoRa.endPacket();
        }
      } else {
        Serial.print("Error sending HTTP POST: ");
        Serial.println(http.errorToString(httpResponseCode).c_str());
      }

      http.end(); // Free resources
    } else {
      Serial.println("WiFi Disconnected. Cannot push to cloud!");
      // Reconnection logic could go here
    }
  }
}
