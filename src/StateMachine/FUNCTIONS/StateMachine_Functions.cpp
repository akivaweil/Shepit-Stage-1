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
const unsigned long MOTOR_TIMEOUT_MS = 3000; // 3 seconds for sleep mode
const unsigned long MOTOR_ENABLE_DELAY_MS = 750; // 750ms motor enable delay

// Motor enable delay tracking
unsigned long motorEnableStartTime = 0;
bool waitingForMotorEnable = false;

// Cutting state tracking
CuttingPhase currentCuttingPhase = CUT_FORWARD_PHASE;

// Emergency stop tracking
unsigned long cycleStartTime = 0;
bool emergencyStopRequested = false;
const unsigned long EMERGENCY_STOP_DELAY_MS = 300; // 300ms delay to prevent accidental stops

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
  }
  resetMotorTimeout();
}

bool isMotorEnableDelayComplete() {
  if (waitingForMotorEnable) {
    if (millis() - motorEnableStartTime >= MOTOR_ENABLE_DELAY_MS) {
      waitingForMotorEnable = false;
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
    Serial.println("*** SLEEP MODE - All motors DISABLED after 3 seconds of idle ***");
  }
}

void checkMotorTimeout() {
  // Sleep mode: disable motors after 3 seconds in idle state
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
//* *********************** PNEUMATIC CLAMP FUNCTIONS *********************
//* ************************************************************************

void extendClamp() {
  digitalWrite(CLAMP_RELAY_PIN, LOW);  // LOW = extended
  // No serial output during motor movement per user rules
}

void retractClamp() {
  digitalWrite(CLAMP_RELAY_PIN, HIGH); // HIGH = retracted  
  // No serial output during motor movement per user rules
}

bool isClampRetracted() {
  return digitalRead(CLAMP_RELAY_PIN) == HIGH;
}

//* ************************************************************************
//* *********************** STATE MACHINE FUNCTIONS **********************
//* ************************************************************************

String getCurrentStateName() {
  switch (currentSystemState) {
    case STATE_IDLE: return "IDLE";
    case STATE_RELOADING: return "RELOADING";
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
  return (currentSystemState == STATE_RELOADING ||
          currentSystemState == STATE_CUTTING || 
          currentSystemState == STATE_RETURNING ||
          currentSystemState == STATE_FEEDING);
}

void handleEmergencyStop() {
  // Stop all motors immediately
  if (feedMotor) feedMotor->forceStop();
  if (cutMotor) cutMotor->forceStop();
  
  // Set emergency stop flag
  emergencyStopRequested = true;
  
  Serial.println("*** EMERGENCY STOP - Returning to home position ***");
  
  // Transition directly to IDLE state
  transitionToState(STATE_IDLE);
}

void transitionToState(SystemState newState) {
  if (newState != currentSystemState) {
    Serial.println("→ " + String(
                   (newState == STATE_IDLE ? "IDLE" : 
                    newState == STATE_RELOADING ? "RELOADING" :
                    newState == STATE_CUTTING ? "CUTTING" : 
                    newState == STATE_RETURNING ? "RETURNING" :
                    newState == STATE_FEEDING ? "FEEDING" : 
                    newState == STATE_MANUAL ? "MANUAL" : "UNKNOWN")));
    
    // Exit current state
    switch (currentSystemState) {
      case STATE_IDLE: exitIdleState(); break;
      case STATE_RELOADING: exitReloadingState(); break;
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
      case STATE_RELOADING: enterReloadingState(); break;
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
    case STATE_RELOADING: updateReloadingState(); break;
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