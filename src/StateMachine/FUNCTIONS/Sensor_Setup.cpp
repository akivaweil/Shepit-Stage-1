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
  feedDistanceSensor.attach(WOOD_DISTANCE_SENSOR_PIN, INPUT);
  feedDistanceSensor.interval(distanceSensorDebounceTime); // Distance sensor debounce
}

void initializeWoodPresenceSensor() {
  // Initialize wood presence sensor with proper debouncing
  woodPresenceSensor.attach(WOOD_PRESENT_SENSOR_PIN, INPUT);
  woodPresenceSensor.interval(distanceSensorDebounceTime); // Use same debounce time
}

void initializeRunCycleSwitch() {
  // Initialize run cycle switch with proper debouncing
  runCycleSwitch.attach(RUN_CYCLE_SWITCH_PIN, INPUT);
  runCycleSwitch.interval(sensorDebounceTime); // Standard sensor debounce
}

void initializeRightSwitch() {
  // Initialize right switch with proper debouncing
  rightSwitch.attach(RIGHT_SWITCH_PIN, INPUT);
  rightSwitch.interval(sensorDebounceTime); // Standard sensor debounce
}

void initializeRedButton() {
  // Initialize red button with proper debouncing
  redButton.attach(RED_BUTTON_PIN, INPUT);
  redButton.interval(sensorDebounceTime); // Standard sensor debounce
}

void initializeAllSensors() {
  // Initialize all sensors at once
  initializeFeedDistanceSensor();
  initializeWoodPresenceSensor();
  initializeRunCycleSwitch();
  initializeRightSwitch();
  initializeRedButton();
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
  return rightSwitch.read() == HIGH;
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
