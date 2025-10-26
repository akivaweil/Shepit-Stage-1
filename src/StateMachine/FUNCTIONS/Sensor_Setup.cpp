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
Bounce2::Button reloadSwitch = Bounce2::Button();
Bounce2::Button redButton = Bounce2::Button();



//* ************************************************************************
//* *********************** SENSOR INITIALIZATION **************************
//* ************************************************************************

void initializeFeedDistanceSensor() {
  // Initialize distance sensor with INPUT mode (active LOW - LOW when wood detected)
  pinMode(WOOD_DISTANCE_SENSOR_PIN, INPUT_PULLUP); // Set pin mode with internal pullup (active LOW)
  feedDistanceSensor.attach(WOOD_DISTANCE_SENSOR_PIN, INPUT);
  feedDistanceSensor.interval(distanceSensorDebounceTime); // Distance sensor debounce
  
  // Minimal log
  Serial.println("Init: Distance sensor");
}



void initializeWoodPresenceSensor() {
  // Initialize wood presence sensor with proper debouncing
  pinMode(WOOD_PRESENT_SENSOR_PIN, INPUT_PULLUP); // Set pin mode with internal pullup (active LOW)
  woodPresenceSensor.attach(WOOD_PRESENT_SENSOR_PIN, INPUT);
  woodPresenceSensor.interval(distanceSensorDebounceTime); // Use same debounce time
  
  // Minimal log
  Serial.println("Init: Wood presence sensor");
}

void initializeRunCycleSwitch() {
  // Initialize run cycle switch with proper debouncing
  pinMode(RUN_CYCLE_SWITCH_PIN, INPUT_PULLDOWN); // Set pin mode with internal pulldown
  runCycleSwitch.attach(RUN_CYCLE_SWITCH_PIN, INPUT);
  runCycleSwitch.interval(sensorDebounceTime); // Standard sensor debounce
  
  // Minimal log
  Serial.println("Init: Run cycle switch");
}

void initializeReloadSwitch() {
  // Initialize reload switch with proper debouncing
  pinMode(RELOAD_SWITCH_PIN, INPUT_PULLDOWN); // Set pin mode with internal pulldown
  reloadSwitch.attach(RELOAD_SWITCH_PIN, INPUT);
  reloadSwitch.interval(sensorDebounceTime); // Standard sensor debounce
  
  // Minimal log
  Serial.println("Init: Reload switch");
}

void initializeRedButton() {
  // Initialize red button with proper debouncing
  pinMode(RED_BUTTON_PIN, INPUT_PULLDOWN); // Set pin mode with internal pulldown
  redButton.attach(RED_BUTTON_PIN, INPUT);
  redButton.interval(sensorDebounceTime); // Standard sensor debounce
  
  // Minimal log
  Serial.println("Init: Red button");
}

void initializeCutMotorHomeSwitch() {
  // Initialize cut motor home switch with direct digital reading (no debouncing)
  pinMode(CUT_MOTOR_HOME_SWITCH_PIN, INPUT); // Set pin mode without internal pull (active HIGH)
  
  // Minimal log
  Serial.println("Init: Cut motor home switch (direct reading)");
}

void initializeAllSensors() {
  // Initialize all sensors at once
  Serial.println("Initializing sensors...");
  
  initializeFeedDistanceSensor();
  
  
  initializeWoodPresenceSensor();
  
  
  initializeRunCycleSwitch();
  
  
  initializeReloadSwitch();
  
  
  initializeRedButton();
  
  initializeCutMotorHomeSwitch();
  
  Serial.println("Sensors initialized");
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

void updateReloadSwitch() {
  reloadSwitch.update();
  
  // No periodic debug here to reduce noise
}

void updateRedButton() {
  redButton.update();
}

// Cut motor home switch now uses direct digital reading - no update function needed

void updateAllSensors() {
  // Update all sensors at once
  updateFeedDistanceSensor();
  updateWoodPresenceSensor();
  updateRunCycleSwitch();
  updateReloadSwitch();
  updateRedButton();
  // Cut motor home switch now uses direct digital reading - no update needed
}

//* ************************************************************************
//* *********************** SENSOR READ FUNCTIONS *************************
//* ************************************************************************

bool isFeedDistanceSensorTriggered() {
  return feedDistanceSensor.read() == LOW;
}



bool isWoodPresenceSensorActive() {
  // Wood presence sensor is active LOW - returns true when wood is detected
  return woodPresenceSensor.read() == LOW;
}

bool isReloadSwitchActive() {
  bool switchState = reloadSwitch.read() == HIGH;
  
  return switchState;
}

bool isRunCycleSwitchActiveCentralized() {
  // Run cycle switch is active HIGH - returns true when switch is ON
  return runCycleSwitch.read() == HIGH;
}

bool isRedButtonPressed() {
  return redButton.read() == HIGH;
}

bool isCutMotorHomeSwitchTriggered() {
  // Cut motor home switch is active HIGH - returns true when at home position
  // Use direct digital reading (no debouncing)
  return digitalRead(CUT_MOTOR_HOME_SWITCH_PIN) == HIGH;
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

void resetReloadSwitch() {
  // Bounce2 doesn't have a reset method, just reinitialize
  reloadSwitch.attach(RELOAD_SWITCH_PIN, INPUT);
  reloadSwitch.interval(sensorDebounceTime);
}

void resetRedButton() {
  // Bounce2 doesn't have a reset method, just reinitialize
  redButton.attach(RED_BUTTON_PIN, INPUT);
  redButton.interval(sensorDebounceTime);
}

void resetCutMotorHomeSwitch() {
  // Cut motor home switch uses direct digital reading - just reinitialize pin mode
  pinMode(CUT_MOTOR_HOME_SWITCH_PIN, INPUT);
}

void resetAllSensors() {
  // Reset all sensors
  resetFeedDistanceSensor();
  resetWoodPresenceSensor();
  resetRunCycleSwitch();
  resetReloadSwitch();
  resetRedButton();
  resetCutMotorHomeSwitch();
}
