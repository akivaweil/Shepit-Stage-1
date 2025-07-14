#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* *********************** GLOBAL STATE VARIABLES ************************
//* ************************************************************************
SystemState currentSystemState = STATE_IDLE;
SystemState previousSystemState = STATE_IDLE;
unsigned long lastActivityTime = 0;
bool motorsEnabled = false;
bool manualMode = false;
const unsigned long MOTOR_TIMEOUT_MS = 2000; // 2 seconds
const unsigned long MOTOR_ENABLE_DELAY_MS = 500; // 500ms motor enable delay

// Motor enable delay tracking
unsigned long motorEnableStartTime = 0;
bool waitingForMotorEnable = false;

// Cutting state tracking
CuttingPhase currentCuttingPhase = CUT_FORWARD_PHASE;

//* ************************************************************************
//* *********************** MOTOR ENABLE FUNCTIONS ************************
//* ************************************************************************

void resetMotorTimeout() {
  lastActivityTime = millis();
}

void enableAllMotors() {
  if (!motorsEnabled) {
    digitalWrite(FEED_MOTOR_ENABLE_PIN, LOW);  // Active low enable
    digitalWrite(CUT_MOTOR_ENABLE_PIN, LOW);   // Active low enable
    motorsEnabled = true;
    Serial.println("All motors ENABLED");
  }
  resetMotorTimeout();
}

void enableAllMotorsWithDelay() {
  if (!motorsEnabled) {
    digitalWrite(FEED_MOTOR_ENABLE_PIN, LOW);  // Active low enable
    digitalWrite(CUT_MOTOR_ENABLE_PIN, LOW);   // Active low enable
    motorsEnabled = true;
    waitingForMotorEnable = true;
    motorEnableStartTime = millis();
    Serial.println("All motors ENABLED - waiting 500ms for stabilization");
  }
  resetMotorTimeout();
}

bool isMotorEnableDelayComplete() {
  if (waitingForMotorEnable) {
    if (millis() - motorEnableStartTime >= MOTOR_ENABLE_DELAY_MS) {
      waitingForMotorEnable = false;
      Serial.println("Motor enable delay complete - ready for movement");
      return true;
    }
    return false;
  }
  return true; // No delay needed
}

void disableAllMotorsAfterDelay() {
  if (motorsEnabled) {
    digitalWrite(FEED_MOTOR_ENABLE_PIN, HIGH); // Active low enable
    digitalWrite(CUT_MOTOR_ENABLE_PIN, HIGH);  // Active low enable
    motorsEnabled = false;
    Serial.println("All motors DISABLED due to timeout");
  }
}

void checkMotorTimeout() {
  // Only check timeout in idle state
  if (currentSystemState == STATE_IDLE) {
    if (motorsEnabled && (millis() - lastActivityTime >= MOTOR_TIMEOUT_MS)) {
      disableAllMotorsAfterDelay();
    }
  }
}

// Individual motor control functions maintained for manual commands
void enableFeedMotor() {
  digitalWrite(FEED_MOTOR_ENABLE_PIN, LOW);  // Active low enable
  resetMotorTimeout();
}

void disableFeedMotor() {
  digitalWrite(FEED_MOTOR_ENABLE_PIN, HIGH); // Active low enable
}

void enableCutMotor() {
  digitalWrite(CUT_MOTOR_ENABLE_PIN, LOW);   // Active low enable
  resetMotorTimeout();
}

void disableCutMotor() {
  digitalWrite(CUT_MOTOR_ENABLE_PIN, HIGH);  // Active low enable
}

//* ************************************************************************
//* *********************** STATE MACHINE FUNCTIONS **********************
//* ************************************************************************

String getCurrentStateName() {
  switch (currentSystemState) {
    case STATE_IDLE: return "IDLE";
    case STATE_CUTTING: return "CUTTING";
    case STATE_RETURNING: return "RETURNING";
    case STATE_FEEDING: return "FEEDING";
    case STATE_MANUAL: return "MANUAL";
    default: return "UNKNOWN";
  }
}

bool isSystemIdle() {
  return currentSystemState == STATE_IDLE;
}

bool isSystemBusy() {
  return (currentSystemState == STATE_CUTTING || 
          currentSystemState == STATE_RETURNING ||
          currentSystemState == STATE_FEEDING);
}

void transitionToState(SystemState newState) {
  if (newState != currentSystemState) {
    Serial.println("*** TRANSITIONING FROM " + getCurrentStateName() + " TO " + 
                   (newState == STATE_IDLE ? "IDLE" : 
                    newState == STATE_CUTTING ? "CUTTING" : 
                    newState == STATE_RETURNING ? "RETURNING" :
                    newState == STATE_FEEDING ? "FEEDING" : 
                    newState == STATE_MANUAL ? "MANUAL" : "UNKNOWN") + " ***");
    
    // Exit current state
    switch (currentSystemState) {
      case STATE_IDLE: exitIdleState(); break;
      case STATE_CUTTING: exitCuttingState(); break;
      case STATE_RETURNING: exitReturningState(); break;
      case STATE_FEEDING: exitFeedingState(); break;
      case STATE_MANUAL: exitManualState(); break;
    }
    
    previousSystemState = currentSystemState;
    currentSystemState = newState;
    
    // Enter new state
    switch (currentSystemState) {
      case STATE_IDLE: enterIdleState(); break;
      case STATE_CUTTING: enterCuttingState(); break;
      case STATE_RETURNING: enterReturningState(); break;
      case STATE_FEEDING: enterFeedingState(); break;
      case STATE_MANUAL: enterManualState(); break;
    }
  }
}

void updateStateMachine() {
  // Update the current state
  switch (currentSystemState) {
    case STATE_IDLE: updateIdleState(); break;
    case STATE_CUTTING: updateCuttingState(); break;
    case STATE_RETURNING: updateReturningState(); break;
    case STATE_FEEDING: updateFeedingState(); break;
    case STATE_MANUAL: updateManualState(); break;
  }
}

void initializeStateMachine() {
  Serial.println("=== INITIALIZING STATE MACHINE ===");
  
  // Initialize variables
  lastActivityTime = millis();
  motorsEnabled = true;  // Motors are permanently enabled
  manualMode = false;
  
  // Start in idle state
  currentSystemState = STATE_IDLE;
  previousSystemState = STATE_IDLE;
  
  // Initialize the idle state
  enterIdleState();
  
  Serial.println("State machine initialized - starting in IDLE state");
}