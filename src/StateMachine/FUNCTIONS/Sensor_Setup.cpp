#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"
#include <Bounce2.h>

//* ************************************************************************
//* *********************** SENSOR SETUP & DEBOUNCING **********************
//* ************************************************************************
// This file contains all sensor initialization and debouncing functions
// that are used across multiple states to avoid code duplication

// Global sensor debouncer objects
Bounce2::Button feedDistanceSensor = Bounce2::Button();
Bounce2::Button woodPresenceSensor = Bounce2::Button();
Bounce2::Button runCycleSwitch = Bounce2::Button();
Bounce2::Button rightSwitch = Bounce2::Button();
Bounce2::Button redButton = Bounce2::Button();

//* ************************************************************************
//* *********************** SENSOR INITIALIZATION **************************
//* ************************************************************************

void initializeFeedDistanceSensor() {
  // Initialize distance sensor with INPUT mode (active HIGH - HIGH when wood detected)
  pinMode(WOOD_DISTANCE_SENSOR_PIN, INPUT_PULLDOWN); // Set pin mode with internal pulldown
  feedDistanceSensor.attach(WOOD_DISTANCE_SENSOR_PIN, INPUT);
  feedDistanceSensor.interval(distanceSensorDebounceTime); // Distance sensor debounce
  
  // Debug: Log initialization
  Serial.println("Feed distance sensor initialized on pin " + String(WOOD_DISTANCE_SENSOR_PIN) + 
                 " with debounce time " + String(distanceSensorDebounceTime) + "ms" +
                 " (INPUT_PULLDOWN mode)");
}

void initializeWoodPresenceSensor() {
  // Initialize wood presence sensor with proper debouncing
  pinMode(WOOD_PRESENT_SENSOR_PIN, INPUT_PULLUP); // Set pin mode with internal pullup (active LOW)
  woodPresenceSensor.attach(WOOD_PRESENT_SENSOR_PIN, INPUT);
  woodPresenceSensor.interval(distanceSensorDebounceTime); // Use same debounce time
  
  // Debug: Log initialization
  Serial.println("Wood presence sensor initialized on pin " + String(WOOD_PRESENT_SENSOR_PIN) + 
                 " with debounce time " + String(distanceSensorDebounceTime) + "ms" +
                 " (INPUT_PULLUP mode - active LOW)");
}

void initializeRunCycleSwitch() {
  // Initialize run cycle switch with proper debouncing
  pinMode(RUN_CYCLE_SWITCH_PIN, INPUT_PULLDOWN); // Set pin mode with internal pulldown
  runCycleSwitch.attach(RUN_CYCLE_SWITCH_PIN, INPUT);
  runCycleSwitch.interval(sensorDebounceTime); // Standard sensor debounce
  
  // Debug: Log initialization
  Serial.println("Run cycle switch initialized on pin " + String(RUN_CYCLE_SWITCH_PIN) + 
                 " with debounce time " + String(sensorDebounceTime) + "ms" +
                 " (INPUT_PULLDOWN mode)");
}

void initializeRightSwitch() {
  // Initialize right switch with proper debouncing
  pinMode(RIGHT_SWITCH_PIN, INPUT_PULLDOWN); // Set pin mode with internal pulldown
  rightSwitch.attach(RIGHT_SWITCH_PIN, INPUT);
  rightSwitch.interval(sensorDebounceTime); // Standard sensor debounce
  
  // Debug: Log initialization
  Serial.println("Right switch initialized on pin " + String(RIGHT_SWITCH_PIN) + 
                 " with debounce time " + String(sensorDebounceTime) + "ms" +
                 " (INPUT_PULLDOWN mode)");
}

void initializeRedButton() {
  // Initialize red button with proper debouncing
  pinMode(RED_BUTTON_PIN, INPUT_PULLDOWN); // Set pin mode with internal pulldown
  redButton.attach(RED_BUTTON_PIN, INPUT);
  redButton.interval(sensorDebounceTime); // Standard sensor debounce
  
  // Debug: Log initialization
  Serial.println("Red button initialized on pin " + String(RED_BUTTON_PIN) + 
                 " with debounce time " + String(sensorDebounceTime) + "ms" +
                 " (INPUT_PULLDOWN mode)");
}

void initializeAllSensors() {
  // Initialize all sensors at once
  Serial.println("Starting sensor initialization...");
  
  initializeFeedDistanceSensor();
  Serial.println("Feed distance sensor initialized");
  
  initializeWoodPresenceSensor();
  Serial.println("Wood presence sensor initialized");
  
  initializeRunCycleSwitch();
  Serial.println("Run cycle switch initialized");
  
  initializeRightSwitch();
  Serial.println("Right switch (reload) initialized");
  
  initializeRedButton();
  Serial.println("Red button initialized");
  
  Serial.println("All sensors initialized successfully");
}

//* ************************************************************************
//* *********************** SENSOR UPDATE FUNCTIONS ***********************
//* ************************************************************************

void updateFeedDistanceSensor() {
  feedDistanceSensor.update();
}

void updateWoodPresenceSensor() {
  woodPresenceSensor.update();
}

void updateRunCycleSwitch() {
  runCycleSwitch.update();
}

void updateRightSwitch() {
  rightSwitch.update();
  
  // Debug: Log switch state changes
  static bool lastRightSwitchState = false;
  bool currentRightSwitchState = rightSwitch.read() == HIGH;
  
  if (currentRightSwitchState != lastRightSwitchState) {
    Serial.println("Right switch state changed to: " + String(currentRightSwitchState ? "ACTIVE" : "INACTIVE"));
    lastRightSwitchState = currentRightSwitchState;
  }
  
  // Additional debug: Log update frequency
  static unsigned long lastUpdateDebug = 0;
  if (millis() - lastUpdateDebug >= 10000) { // Log every 10 seconds
    Serial.println("Right switch update frequency check - being updated every ~10ms");
    lastUpdateDebug = millis();
  }
}

void updateRedButton() {
  redButton.update();
}

void updateAllSensors() {
  // Update all sensors at once
  updateFeedDistanceSensor();
  updateWoodPresenceSensor();
  updateRunCycleSwitch();
  updateRightSwitch();
  updateRedButton();
  
  // Debug: Log sensor update frequency
  static unsigned long lastAllSensorsDebug = 0;
  if (millis() - lastAllSensorsDebug >= 15000) { // Log every 15 seconds
    Serial.println("All sensors updated - frequency check");
    lastAllSensorsDebug = millis();
  }
}

//* ************************************************************************
//* *********************** SENSOR READ FUNCTIONS *************************
//* ************************************************************************

bool isFeedDistanceSensorTriggered() {
  return feedDistanceSensor.read() == HIGH;
}

bool isWoodPresenceSensorActive() {
  // Wood presence sensor is active LOW - returns true when wood is detected
  return woodPresenceSensor.read() == LOW;
}

bool isRightSwitchActive() {
  bool switchState = rightSwitch.read() == HIGH;
  
  // Debug: Log switch reads occasionally
  static unsigned long lastSwitchReadDebug = 0;
  if (millis() - lastSwitchReadDebug >= 5000) { // Log every 5 seconds
    Serial.println("Right switch read: " + String(switchState ? "ACTIVE" : "INACTIVE") + 
                   " (raw: " + String(rightSwitch.read()) + ")");
    lastSwitchReadDebug = millis();
  }
  
  // Additional debug: Log every switch read for troubleshooting
  static bool lastSwitchState = false;
  if (switchState != lastSwitchState) {
    Serial.println("Right switch state change detected: " + String(switchState ? "ACTIVE" : "INACTIVE"));
    lastSwitchState = switchState;
  }
  
  // Additional debug: Log raw pin value for comparison
  static unsigned long lastRawPinDebug = 0;
  if (millis() - lastRawPinDebug >= 3000) { // Log every 3 seconds
    int rawPinValue = digitalRead(RIGHT_SWITCH_PIN);
    Serial.println("Right switch comparison - Bounce2: " + String(rightSwitch.read()) + 
                   ", Raw pin: " + String(rawPinValue) + 
                   ", Processed: " + String(switchState ? "ACTIVE" : "INACTIVE"));
    lastRawPinDebug = millis();
  }
  
  return switchState;
}

bool isRunCycleSwitchActiveCentralized() {
  // Run cycle switch is active HIGH - returns true when switch is ON
  return runCycleSwitch.read() == HIGH;
}

bool isRedButtonPressed() {
  return redButton.read() == HIGH;
}

//* ************************************************************************
//* *********************** SENSOR RESET FUNCTIONS ************************
//* ************************************************************************

void resetFeedDistanceSensor() {
  // Bounce2 doesn't have a reset method, just reinitialize
  feedDistanceSensor.attach(WOOD_DISTANCE_SENSOR_PIN, INPUT);
  feedDistanceSensor.interval(distanceSensorDebounceTime);
}

void resetWoodPresenceSensor() {
  // Bounce2 doesn't have a reset method, just reinitialize
  woodPresenceSensor.attach(WOOD_PRESENT_SENSOR_PIN, INPUT);
  woodPresenceSensor.interval(distanceSensorDebounceTime);
}

void resetRunCycleSwitch() {
  // Bounce2 doesn't have a reset method, just reinitialize
  runCycleSwitch.attach(RUN_CYCLE_SWITCH_PIN, INPUT);
  runCycleSwitch.interval(sensorDebounceTime);
}

void resetRightSwitch() {
  // Bounce2 doesn't have a reset method, just reinitialize
  rightSwitch.attach(RIGHT_SWITCH_PIN, INPUT);
  rightSwitch.interval(sensorDebounceTime);
}

void resetRedButton() {
  // Bounce2 doesn't have a reset method, just reinitialize
  redButton.attach(RED_BUTTON_PIN, INPUT);
  redButton.interval(sensorDebounceTime);
}

void resetAllSensors() {
  // Reset all sensors
  resetFeedDistanceSensor();
  resetWoodPresenceSensor();
  resetRunCycleSwitch();
  resetRightSwitch();
  resetRedButton();
}
