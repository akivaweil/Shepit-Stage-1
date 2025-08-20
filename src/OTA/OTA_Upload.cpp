#include "OTA_Upload.h"
#include <WiFi.h>
#include <ArduinoOTA.h>

//* ************************************************************************
//* *********************** OTA UPLOAD IMPLEMENTATION *********************
//* ************************************************************************
// Barebones WiFi connection and Over-The-Air updates for the ESP32.

const char* ssid = "Everwood";
const char* password = "Everwood-Staff";

void setupOTA() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.setHostname("stage1-esp32s3");
  ArduinoOTA.begin();
}

void handleOTA() {
  ArduinoOTA.handle();
} 