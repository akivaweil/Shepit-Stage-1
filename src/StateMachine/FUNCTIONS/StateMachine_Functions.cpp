#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

// External motor objects from main.cpp
extern FastAccelStepper *feedMotor;
extern FastAccelStepper *cutMotor;

// Include state implementations
#include "../STATES/00_IDLE.cpp"
#include "../STATES/01_HOMING.cpp"
#include "../STATES/cut_cycle/02_LOAD.cpp"
#include "../STATES/cut_cycle/03_CUTTING.cpp"
#include "../STATES/04_UNLOAD.cpp"

//* ************************************************************************
//* *********************** GLOBAL STATE VARIABLES ************************
//* ************************************************************************
SystemState currentSystemState = STATE_IDLE;
SystemState previousSystemState = STATE_IDLE;
unsigned long lastActivityTime = 0;
bool motorsEnabled = false;

// Motor timeout and enable delay moved to Config.h

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
//* *********************** STARTUP SAFETY FUNCTIONS **********************
//* ************************************************************************
// Startup safety: prevents automatic cycle start if run cycle switch is on at startup
// This requires the user to turn the switch off and back on to reset the safety
bool startupSafetyResetRequired = false;

void resetStartupSafety() {
  startupSafetyResetRequired = false;
  Serial.println("Startup safety RESET - system can now start cycles");
}

bool isStartupSafetyResetRequired() {
  return startupSafetyResetRequired;
}

//* ************************************************************************
//* *********************** MOTOR ENABLE FUNCTIONS ************************
//* ************************************************************************

void resetMotorTimeout() {
  lastActivityTime = millis();
}

void enableAllMotors() {
  // Always write enable pins to ensure motors are enabled
  digitalWrite(FEED_MOTOR_ENABLE_PIN, LOW);  // Active low enable - enable feed motor
  digitalWrite(CUT_MOTOR_ENABLE_PIN, LOW);   // Active low enable - ensure cut motor is enabled
  motorsEnabled = true;
  resetMotorTimeout();
}

void enableAllMotorsWithDelay() {
  if (!motorsEnabled) {
    digitalWrite(FEED_MOTOR_ENABLE_PIN, LOW);  // Active low enable - enable feed motor
    digitalWrite(CUT_MOTOR_ENABLE_PIN, LOW);   // Active low enable - ensure cut motor is enabled
    motorsEnabled = true;
    waitingForMotorEnable = true;
    motorEnableStartTime = millis();
  }
  resetMotorTimeout();
}

bool isMotorEnableDelayComplete() {
  if (waitingForMotorEnable) {
    if (millis() - motorEnableStartTime >= motorEnableDelayMs) {
      waitingForMotorEnable = false;
      return true;
    }
    return false;
  }
  return true; // No delay needed
}

void disableAllMotorsAfterDelay() {
  if (motorsEnabled) {
    digitalWrite(FEED_MOTOR_ENABLE_PIN, HIGH); // Active low enable - disable feed motor
    digitalWrite(CUT_MOTOR_ENABLE_PIN, LOW);   // Active low enable - ensure cut motor stays enabled
    // Cut motor is NEVER disabled - always enabled
    motorsEnabled = false;
    Serial.println("Sleep mode: feed motor disabled (cut motor always enabled)");
  }
}

void checkMotorTimeout() {
  // Sleep mode: disable motors after 3 seconds in idle state
  // BUT only if feed motor is not running (to prevent relay flickering)
  // Also don't disable motors during unload state
  // Cut motor is ALWAYS enabled
  digitalWrite(CUT_MOTOR_ENABLE_PIN, LOW);  // Always keep cut motor enabled
  
  if (currentSystemState == STATE_IDLE) {
    // Check if feed motor is currently running
    bool feedMotorRunning = (feedMotor && feedMotor->isRunning());
    
    // Only disable motors if they're enabled AND feed motor is not running AND timeout exceeded
    if (motorsEnabled && !feedMotorRunning && (millis() - lastActivityTime >= motorTimeoutMs)) {
      disableAllMotorsAfterDelay();
    }
  }
  // Note: Motors remain enabled during unload state to prevent interruption
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
  // Cut motor is NEVER disabled - always keep it enabled
  digitalWrite(CUT_MOTOR_ENABLE_PIN, LOW);  // Active low enable - keep cut motor enabled
}

//* ************************************************************************
//* *********************** FORWARD CLAMP FUNCTIONS *********************
//* ************************************************************************

void extendForwardClamp() {
  digitalWrite(CLAMP_RELAY_PIN, LOW);  // LOW = extended
  // No serial output during motor movement per user rules
}

void retractForwardClamp() {
  digitalWrite(CLAMP_RELAY_PIN, HIGH); // HIGH = retracted  
  // No serial output during motor movement per user rules
}

bool isForwardClampRetracted() {
  return digitalRead(CLAMP_RELAY_PIN) == HIGH;
}

//* ************************************************************************
//* *********************** WOOD SENSOR FUNCTIONS *************************
//* ************************************************************************

bool isWoodPresent() {
  // Wood present sensor is active LOW - returns true when wood is detected
  // Use centralized sensor function for reliable detection
  return isWoodPresenceSensorActive();
}

//* ************************************************************************
//* *********************** WOOD DISTANCE SENSOR FUNCTIONS *****************
//* ************************************************************************

bool isWoodAtCorrectDistance() {
  // Use the distance sensor (pin 10) for wood distance detection
  // This sensor has debouncing for reliable detection
  return isFeedDistanceSensorTriggered();
}

//* ************************************************************************
//* *********************** RUN CYCLE SWITCH FUNCTIONS ********************
//* ************************************************************************

bool isRunCycleSwitchActive() {
  // Run cycle switch is active HIGH - returns true when switch is ON
  // Use centralized sensor function for reliable detection
  return isRunCycleSwitchActiveCentralized();
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
    
    // Retract forward clamp before feed motor movement
    retractForwardClamp();
    
    // Add 50ms delay to ensure clamp is fully retracted before motor movement
    delay(50);
    
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
    
    // Extend forward clamp when feed motor stops
    extendForwardClamp();
    
    Serial.println("Continuous feed stopped - motor stopped and forward clamp extended");
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
    case STATE_HOMING: return "HOMING";
    case STATE_LOAD: return "LOAD";
    case STATE_CUTTING: return "CUTTING";
    case STATE_UNLOAD: return "UNLOAD";
    default: return "UNKNOWN";
  }
}

SystemState getCurrentState() {
  return currentSystemState;
}

bool isSystemIdle() {
  return currentSystemState == STATE_IDLE;
}

bool isSystemBusy() {
  return (currentSystemState == STATE_HOMING ||
          currentSystemState == STATE_LOAD ||
          currentSystemState == STATE_CUTTING ||
          currentSystemState == STATE_UNLOAD);
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
  // Exit current state
  switch (currentSystemState) {
    case STATE_IDLE:
      // Exit handled by enter function
      break;
    case STATE_HOMING:
      exitHomingState();
      break;
    case STATE_LOAD:
      // Exit handled by enter function
      break;
    case STATE_CUTTING:
      // Exit handled by enter function
      break;
    case STATE_UNLOAD:
      // Exit handled by enter function
      break;
  }
  
  // Update state tracking
  previousSystemState = currentSystemState;
  currentSystemState = newState;
  
  // Enter new state
  switch (newState) {
    case STATE_IDLE:
      enterIdleState();
      break;
    case STATE_HOMING:
      enterHomingState();
      break;
    case STATE_LOAD:
      enterLoadState();
      break;
    case STATE_CUTTING:
      enterCuttingState();
      break;
    case STATE_UNLOAD:
      enterUnloadState();
      break;
  }
}

void updateStateMachine() {
  // Update all sensors first (debouncing)
  updateAllSensors();
  
  // Update current state
  switch (currentSystemState) {
    case STATE_IDLE:
      updateIdleState();
      break;
    case STATE_HOMING:
      updateHomingState();
      break;
    case STATE_LOAD:
      updateLoadState();
      break;
    case STATE_CUTTING:
      updateCuttingState();
      break;
    case STATE_UNLOAD:
      updateUnloadState();
      break;
  }
}

void initializeStateMachine() {
  Serial.println("=== INITIALIZING STATE MACHINE ===");
  
  // Initialize variables
  lastActivityTime = millis();
  motorsEnabled = true;  // Motors are permanently enabled
  
  // Initialize all sensors first
  Serial.println("Initializing sensors...");
  initializeAllSensors();
  Serial.println("Sensor initialization complete");
  
  //! ************************************************************************
  //! STARTUP SAFETY CHECK: Check if run cycle switch is already ON at startup
  //! ************************************************************************
  // If the run cycle switch is ON when the machine starts, require a reset
  // This prevents automatic cycle start without user intervention
  if (isRunCycleSwitchActive()) {
    startupSafetyResetRequired = true;
    Serial.println("⚠️  STARTUP SAFETY: Run cycle switch is ON at startup");
    Serial.println("⚠️  STARTUP SAFETY: Turn switch OFF then ON to reset safety");
    Serial.println("⚠️  STARTUP SAFETY: No automatic cycles until reset");
  } else {
    startupSafetyResetRequired = false;
    Serial.println("✓ STARTUP SAFETY: Run cycle switch is OFF - system ready");
  }
  
  // Start in homing state
  currentSystemState = STATE_HOMING;
  previousSystemState = STATE_HOMING;
  
  // Enter initial state
  enterHomingState();
  
  Serial.println("State machine initialized - starting in HOMING state");
}

//* ************************************************************************
//* *********************** STATE IMPLEMENTATIONS **************************
//* ************************************************************************
// State implementations are in the States folder:
// - 00_IDLE.cpp
// - 01_HOMING.cpp
// - 02_LOAD.cpp
// - 03_CUTTING.cpp
// - 04_UNLOAD.cpp

//* ************************************************************************
//* *********************** STATE MACHINE RESET FUNCTIONS *********************
//* ************************************************************************

// External references to state variables that need resetting
extern CuttingStep currentStep; // From CUTTING state

void resetAllStateMachineFlags() {
  Serial.println("=== RESETTING ALL STATE MACHINE FLAGS ===");
  
  // Reset global state machine flags
  emergencyStopRequested = false;
  
  // Reset feed motor timeout lock
  if (feedMotorTimeoutLocked) {
    feedMotorTimeoutLocked = false;
    Serial.println("Feed motor timeout lock RESET");
  }
  
  // Reset startup safety flag
  if (startupSafetyResetRequired) {
    startupSafetyResetRequired = false;
    Serial.println("Startup safety flag RESET");
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
void resetFeedMotorControlVariables(bool stopMotor) {
  Serial.println("=== RESETTING FEED MOTOR CONTROL VARIABLES ===");
  
  // Call the IDLE state function to reset its static variables
  resetIdleFeedMotorControl(stopMotor);
  
  Serial.println("Feed motor control variables reset complete");
}