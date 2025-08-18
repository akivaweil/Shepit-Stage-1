#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

// External motor objects from main.cpp
extern FastAccelStepper *feedMotor;
extern FastAccelStepper *cutMotor;

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

// SIMPLIFIED: Only essential motor enable tracking
unsigned long motorEnableStartTime = 0;
bool waitingForMotorEnable = false;

// SIMPLIFIED: Only essential emergency stop tracking
unsigned long cycleStartTime = 0;
bool emergencyStopRequested = false;
const unsigned long EMERGENCY_STOP_DELAY_MS = 300; // 300ms delay to prevent accidental stops

// Feed motor timeout lock - prevents restart after timeout until manually reset
bool feedMotorTimeoutLocked = false;

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
    Serial.println("Sleep mode: motors disabled (3 second timeout)");
  }
}

void checkMotorTimeout() {
  // Sleep mode: disable motors after 3 seconds in idle state
  // BUT only if feed motor is not running (to prevent relay flickering)
  // Also don't disable motors during reload state
  if (currentSystemState == STATE_IDLE) {
    // Check if feed motor is currently running
    bool feedMotorRunning = (feedMotor && feedMotor->isRunning());
    
    // Only disable motors if they're enabled AND feed motor is not running AND timeout exceeded
    if (motorsEnabled && !feedMotorRunning && (millis() - lastActivityTime >= MOTOR_TIMEOUT_MS)) {
      disableAllMotorsAfterDelay();
    }
  }
  // Note: Motors remain enabled during reload state to prevent interruption
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
//* *********************** WOOD SENSOR FUNCTIONS *************************
//* ************************************************************************

bool isWoodPresent() {
  // Wood present sensor is active LOW - returns true when wood is detected
  return digitalRead(WOOD_PRESENT_SENSOR_PIN) == LOW;
}

//* ************************************************************************
//* *********************** WOOD DISTANCE SENSOR FUNCTIONS *****************
//* ************************************************************************

bool isWoodAtCorrectDistance() {
  // Wood distance sensor is active HIGH - returns true when wood is at correct distance
  return digitalRead(WOOD_DISTANCE_SENSOR_PIN) == HIGH;
}

//* ************************************************************************
//* *********************** RUN CYCLE SWITCH FUNCTIONS ********************
//* ************************************************************************

bool isRunCycleSwitchActive() {
  // Run cycle switch is active HIGH - returns true when switch is ON
  return digitalRead(RUN_CYCLE_SWITCH_PIN) == HIGH;
}

//* ************************************************************************
//* *********************** CONTINUOUS FEED FUNCTIONS *********************
//* ************************************************************************

void startContinuousFeed() {
  if (feedMotor) {
    // Explicitly enable motors if they're disabled (wake from sleep mode)
    if (!motorsEnabled) {
      enableAllMotors();
      Serial.println("Motors enabled for continuous feed");
    }
    
    // Retract clamp before feed motor movement
    retractClamp();
    
    // Start continuous forward movement
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->runForward();
    
    // Reset motor timeout to keep motors enabled
    resetMotorTimeout();
    
    Serial.println("Continuous feed started - motor moving forward");
  }
}

void stopContinuousFeed() {
  if (feedMotor) {
    // Stop the feed motor
    feedMotor->forceStop();
    
    // Extend clamp when feed motor stops
    extendClamp();
    
    Serial.println("Continuous feed stopped - motor stopped and clamp extended");
  }
}

//* ************************************************************************
//* *********************** FEED MOTOR TIMEOUT LOCK FUNCTIONS *********************
//* ************************************************************************

void resetFeedMotorTimeoutLock() {
  feedMotorTimeoutLocked = false;
  Serial.println("Feed motor timeout lock RESET - motor can now restart");
}

bool isFeedMotorTimeoutLocked() {
  return feedMotorTimeoutLocked;
}

void setFeedMotorTimeoutLocked(bool locked) {
  feedMotorTimeoutLocked = locked;
  if (locked) {
    Serial.println("Feed motor timeout lock SET - motor locked until manually reset");
  } else {
    Serial.println("Feed motor timeout lock CLEARED - motor can now restart");
  }
}

//* ************************************************************************
//* *********************** STATE MACHINE FUNCTIONS **********************
//* ************************************************************************

String getCurrentStateName() {
  switch (currentSystemState) {
    case STATE_IDLE: return "IDLE";
    case STATE_FEED_TO_DISTANCE: return "FEED_TO_DISTANCE";
    case STATE_CUTTING: return "CUTTING";
    case STATE_MANUAL: return "MANUAL";
    case STATE_RELOAD: return "RELOAD";
    default: return "UNKNOWN";
  }
}

bool isSystemIdle() {
  return currentSystemState == STATE_IDLE;
}

bool isSystemBusy() {
  return (currentSystemState == STATE_FEED_TO_DISTANCE ||
          currentSystemState == STATE_CUTTING ||
          currentSystemState == STATE_RELOAD);
}

//* ************************************************************************
//* *********************** CUTTING CYCLE CHECK ***************************
//* ************************************************************************
// Check if system is currently in a cutting cycle (cutting state only)
// This prevents feed motor from running during cutting operations

bool isInCuttingCycle() {
  return (currentSystemState == STATE_CUTTING);
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
                    newState == STATE_FEED_TO_DISTANCE ? "FEED_TO_DISTANCE" :
                    newState == STATE_CUTTING ? "CUTTING" : 
                    newState == STATE_MANUAL ? "MANUAL" :
                    newState == STATE_RELOAD ? "RELOAD" : "UNKNOWN")));
    
    // Exit current state
    switch (currentSystemState) {
      case STATE_IDLE: exitIdleState(); break;
      case STATE_FEED_TO_DISTANCE: exitFeedToDistanceState(); break;
      case STATE_CUTTING: exitCuttingState(); break;
      case STATE_MANUAL: exitManualState(); break;
      case STATE_RELOAD: exitReloadState(); break;
    }
    
    previousSystemState = currentSystemState;
    currentSystemState = newState;
    
    // Enter new state
    switch (currentSystemState) {
      case STATE_IDLE: enterIdleState(); break;
      case STATE_FEED_TO_DISTANCE: enterFeedToDistanceState(); break;
      case STATE_CUTTING: enterCuttingState(); break;
      case STATE_MANUAL: enterManualState(); break;
      case STATE_RELOAD: enterReloadState(); break;
    }
  }
}

void updateStateMachine() {
  // Update the current state
  switch (currentSystemState) {
    case STATE_IDLE: updateIdleState(); break;
    case STATE_FEED_TO_DISTANCE: updateFeedToDistanceState(); break;
    case STATE_CUTTING: updateCuttingState(); break;
    case STATE_MANUAL: updateManualState(); break;
    case STATE_RELOAD: updateReloadState(); break;
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

//* ************************************************************************
//* *********************** STATE IMPLEMENTATIONS **************************
//* ************************************************************************
// Include the actual state implementations

// IDLE state
#include "../STATES/00_IDLE.cpp"

// Cut cycle states
#include "../STATES/cut_cycle/02_FEED_TO_DISTANCE.cpp"
#include "../STATES/cut_cycle/03_CUTTING.cpp"

// Manual state
#include "../STATES/06_MANUAL.cpp"

// Reload state
#include "../STATES/04_RELOAD.cpp"

//* ************************************************************************
//* *********************** STATE MACHINE RESET FUNCTIONS *********************
//* ************************************************************************

// External references to state variables that need resetting
extern CuttingStep currentStep; // From CUTTING state
extern int32_t cutStartPosition; // From CUTTING state
extern int32_t returnStartPosition; // From CUTTING state

void resetAllStateMachineFlags() {
  Serial.println("=== RESETTING ALL STATE MACHINE FLAGS ===");
  
  // Reset global state machine flags
  emergencyStopRequested = false;
  manualMode = false;
  
  // Reset feed motor timeout lock
  if (feedMotorTimeoutLocked) {
    feedMotorTimeoutLocked = false;
    Serial.println("Feed motor timeout lock RESET");
  }
  
  // Reset motor enable tracking
  waitingForMotorEnable = false;
  motorEnableStartTime = 0;
  
  // Reset activity tracking
  lastActivityTime = millis();
  
  // Reset cycle tracking
  cycleStartTime = 0;
  
  Serial.println("All state machine flags have been reset");
  Serial.println("System is now in a clean state");
}

// Function to reset cutting cycle specific flags
void resetCuttingCycleFlags() {
  Serial.println("=== RESETTING CUTTING CYCLE FLAGS ===");
  
  // Reset cutting step to beginning
  currentStep = STEP_ACTIVATE_MOTORS;
  
  // Reset motor position tracking
  cutStartPosition = 0;
  returnStartPosition = 0;
  
  Serial.println("Cutting cycle flags reset - ready for new cycle");
}

// Function to reset feed motor related flags
void resetFeedMotorFlags() {
  Serial.println("=== RESETTING FEED MOTOR FLAGS ===");
  
  // Reset feed motor timeout lock
  if (feedMotorTimeoutLocked) {
    feedMotorTimeoutLocked = false;
    Serial.println("Feed motor timeout lock RESET");
  }
  
  Serial.println("Feed motor flags reset");
}

// Function to reset all motor movement flags
void resetMotorMovementFlags() {
  Serial.println("=== RESETTING MOTOR MOVEMENT FLAGS ===");
  
  // Stop any running motors
  if (feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
    Serial.println("Feed motor stopped during reset");
  }
  
  if (cutMotor && cutMotor->isRunning()) {
    cutMotor->forceStop();
    Serial.println("Cut motor stopped during reset");
  }
  
  Serial.println("Motor movement flags reset - all motors stopped");
}

// Function to reset feed motor control variables (IDLE state specific)
void resetFeedMotorControlVariables() {
  Serial.println("=== RESETTING FEED MOTOR CONTROL VARIABLES ===");
  
  // Call the IDLE state function to reset its static variables
  resetIdleFeedMotorControl();
  
  Serial.println("Feed motor control variables reset complete");
}