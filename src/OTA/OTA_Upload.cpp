#include <WiFi.h>
#include <ArduinoOTA.h>
#include "StateMachine.h"

//* ************************************************************************
//* *********************** OTA UPLOAD IMPLEMENTATION *********************
//* ************************************************************************
// Barebones WiFi connection and Over-The-Air updates for the ESP32.
// OTA uploads are only allowed when system is in IDLE or HOMING states.

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
  // Only allow OTA uploads in IDLE or HOMING states
  SystemState currentState = getCurrentState();
  if (currentState == STATE_IDLE || currentState == STATE_HOMING) {
    ArduinoOTA.handle();
  }
} 