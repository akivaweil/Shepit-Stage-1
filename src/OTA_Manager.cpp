#include <WiFi.h>
#include <ArduinoOTA.h>

//* ************************************************************************
//* *********************** OTA MANAGER ***********************************
//* ************************************************************************
// Complete OTA management functionality for ESP32-S3
// All code contained in this single file as per user requirements

const char* ssid = "Everwood";
const char* password = "Everwood-Staff";

void setupOTA() {
  // Configure WiFi connection
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  // Wait for connection with automatic restart on failure
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(5000);
    ESP.restart();
  }
  
  // Print IP address when connected
  Serial.begin(115200);
  Serial.print("WiFi Connected! IP Address: ");
  Serial.println(WiFi.localIP());

  // Configure OTA settings
  ArduinoOTA.setHostname("shepit-s1-cubes");
  
  // Setup OTA callbacks for better debugging
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
  });
  
  ArduinoOTA.onEnd([]() {
    // OTA update completed
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    // Progress callback - no serial output while motors running
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    // Error handling
  });

  // Start OTA service
  ArduinoOTA.begin();
}

void handleOTA() {
  ArduinoOTA.handle();
} 