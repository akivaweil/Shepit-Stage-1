#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

// External state variable reference
extern SystemState currentSystemState;

//* ************************************************************************
//* ************************ IDLE STATE ***********************************
//* ************************************************************************

// Feed motor control state tracking
static bool feedMotorShouldRun = false;
static bool feedMotorWasRunning = false;
static unsigned long lastFeedMotorStateChange = 0;
static const unsigned long FEED_MOTOR_STATE_CHANGE_DELAY = 100;

// Feed motor timeout tracking
static unsigned long feedMotorStartTime = 0;
static bool feedMotorTimeoutOccurred = false;

// Clamp state tracking
static bool clampShouldBeRetracted = false;
static bool clampWasRetracted = false;
static unsigned long lastClampStateChange = 0;
static const unsigned long CLAMP_STATE_CHANGE_DELAY = 100;

void enterIdleState() {
  resetAllStateMachineFlags();
  enableAllMotors();
  resetMotorTimeout();
  
  // Note: Cut motor homed flag is NOT reset here to prevent double homing
  // The homed flag should only be reset during explicit system reset or new homing sequence
  
  feedMotorShouldRun = false;
  feedMotorWasRunning = false;
  lastFeedMotorStateChange = 0;
  feedMotorTimeoutOccurred = false;
  resetFeedMotorTimeoutLock();
  
  clampShouldBeRetracted = false;
  clampWasRetracted = false;
  lastClampStateChange = 0;
  
  if (feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
  }
  
  emergencyStopRequested = false;
}

void updateIdleState() {
  bool runCycleActive = isRunCycleSwitchActive();
  bool woodPresent = isWoodPresent();
  bool distanceSensorTriggered = isWoodAtCorrectDistance();
  bool inCuttingCycle = isInCuttingCycle();
  
  // Log state changes
  static bool lastRunCycleActive = false;
  static bool lastWoodPresent = false;
  if (runCycleActive != lastRunCycleActive || woodPresent != lastWoodPresent) {
    Serial.println("=== IDLE STATE CONDITIONS ===");
    Serial.println("Run cycle switch: " + String(runCycleActive ? "ACTIVE" : "INACTIVE"));
    Serial.println("Wood present: " + String(woodPresent ? "YES" : "NO"));
    Serial.println("Distance sensor: " + String(distanceSensorTriggered ? "TRIGGERED" : "NOT TRIGGERED"));
    Serial.println("In cutting cycle: " + String(inCuttingCycle ? "YES" : "NO"));
    Serial.println("Feed motor timeout locked: " + String(isFeedMotorTimeoutLocked() ? "YES" : "NO"));
    Serial.println("Cut motor homed: " + String(isCutMotorHomed() ? "YES" : "NO"));
    lastRunCycleActive = runCycleActive;
    lastWoodPresent = woodPresent;
  }
  
  // Feed motor runs when run cycle switch is ON and wood is present
  bool newFeedMotorShouldRun = runCycleActive && woodPresent && !inCuttingCycle && !isFeedMotorTimeoutLocked() && (currentSystemState != STATE_RELOAD);
  
  // Reset feed motor timeout lock when conditions change
  if (runCycleActive != lastRunCycleActive || woodPresent != lastWoodPresent) {
    if (isFeedMotorTimeoutLocked()) {
      resetFeedMotorTimeoutLock();
      feedMotorTimeoutOccurred = false;
    }
  }
  
  // Allow user to reset timeout lock by cycling run cycle switch
  static bool wasLockedWhenSwitchOff = false;
  
  if (!runCycleActive && isFeedMotorTimeoutLocked()) {
    if (!wasLockedWhenSwitchOff) {
      wasLockedWhenSwitchOff = true;
    }
  } else if (runCycleActive && isFeedMotorTimeoutLocked()) {
    if (wasLockedWhenSwitchOff) {
      resetFeedMotorTimeoutLock();
      feedMotorTimeoutOccurred = false;
      wasLockedWhenSwitchOff = false;
    }
  } else {
    wasLockedWhenSwitchOff = false;
  }
  
  // Update feed motor should-run state
  if (newFeedMotorShouldRun != feedMotorShouldRun) {
    feedMotorShouldRun = newFeedMotorShouldRun;
    feedMotorWasRunning = !feedMotorShouldRun;
  }
  
  // Force motor state update when conditions change
  static bool lastConditions = false;
  bool currentConditions = runCycleActive && woodPresent && !inCuttingCycle && !isFeedMotorTimeoutLocked() && (currentSystemState != STATE_RELOAD);
  
  if (currentConditions != lastConditions) {
    feedMotorWasRunning = !currentConditions;
    lastConditions = currentConditions;
  }
  
  // Check for cycle switch state changes
  static bool lastRunCycleState = false;
  if (runCycleActive != lastRunCycleState) {
    if (runCycleActive) {
      if (isFeedMotorTimeoutLocked()) {
        resetFeedMotorTimeoutLock();
        feedMotorTimeoutOccurred = false;
        wasLockedWhenSwitchOff = false;
      }
      feedMotorWasRunning = false;
    } else {
      if (feedMotor && feedMotor->isRunning()) {
        feedMotor->forceStop();
        feedMotorWasRunning = false;
        feedMotorTimeoutOccurred = false;
      }
      feedMotorShouldRun = false;
      feedMotorWasRunning = false;
    }
    lastRunCycleState = runCycleActive;
  }
  
  // Determine if clamp should be retracted
  clampShouldBeRetracted = feedMotorShouldRun;
  
  // Control clamp state
  if (clampShouldBeRetracted != clampWasRetracted) {
    if (millis() - lastClampStateChange >= CLAMP_STATE_CHANGE_DELAY) {
      lastClampStateChange = millis();
      
      if (clampShouldBeRetracted) {
        if (!isForwardClampRetracted()) {
          retractForwardClamp();
        }
      } else {
        if (isForwardClampRetracted()) {
          extendForwardClamp();
        }
      }
      
      clampWasRetracted = clampShouldBeRetracted;
    }
  }
  
  // Control feed motor based on should-run state
  if (feedMotorShouldRun != feedMotorWasRunning) {
    Serial.println("=== FEED MOTOR STATE CHANGE ===");
    Serial.println("feedMotorShouldRun: " + String(feedMotorShouldRun ? "TRUE" : "FALSE"));
    Serial.println("feedMotorWasRunning: " + String(feedMotorWasRunning ? "TRUE" : "FALSE"));
    Serial.println("Elapsed time: " + String(millis() - lastFeedMotorStateChange) + "ms");
    
    if (millis() - lastFeedMotorStateChange >= FEED_MOTOR_STATE_CHANGE_DELAY) {
      lastFeedMotorStateChange = millis();
      
      if (feedMotorShouldRun) {
        Serial.println("Attempting to start feed motor...");
        if (feedMotor && !feedMotor->isRunning()) {
          Serial.println("Feed motor pointer valid and not running");
          if (!motorsEnabled) {
            Serial.println("Motors disabled - enabling...");
            enableAllMotors();
          }
          
          feedMotor->setSpeedInHz(feedMotorSpeed);
          feedMotor->setAcceleration(feedMotorAcceleration);
          if (isCutMotorHomed()) {
            Serial.println("Cut motor homed - starting feed motor forward");
            feedMotor->runForward();
            Serial.println("Feed motor runForward() called");
          } else {
            Serial.println("=== FEED MOTOR START BLOCKED ===");
            Serial.println("ERROR: Cut motor not homed - auto-homing before feed operation");
            Serial.println("Cut motor homed flag: " + String(isCutMotorHomed() ? "TRUE" : "FALSE"));
            Serial.println("Cut motor home switch: " + String(isCutMotorHomeSwitchTriggered() ? "TRIGGERED" : "NOT TRIGGERED"));
            Serial.println("Motors enabled: " + String(motorsEnabled ? "YES" : "NO"));
            Serial.println("Setting return state to IDLE and transitioning to HOMING...");
            setReturnStateAfterHoming(STATE_IDLE);
            transitionToState(STATE_HOMING);
          }
          
          if (currentSystemState != STATE_RELOAD) {
            feedMotorStartTime = millis();
            feedMotorTimeoutOccurred = false;
          }
          
          resetMotorTimeout();
        } else {
          Serial.println("Feed motor pointer invalid or already running");
        }
      } else {
        Serial.println("Stopping feed motor...");
        if (feedMotor && feedMotor->isRunning()) {
          feedMotor->forceStop();
          feedMotorTimeoutOccurred = false;
          Serial.println("Feed motor stopped");
        }
      }
      
      feedMotorWasRunning = feedMotorShouldRun;
      Serial.println("feedMotorWasRunning updated to: " + String(feedMotorWasRunning ? "TRUE" : "FALSE"));
    } else {
      Serial.println("Waiting for delay - only " + String(millis() - lastFeedMotorStateChange) + "ms elapsed");
    }
  }
  
  // Monitor conditions and stop feed motor immediately if they change
  if (feedMotor && feedMotor->isRunning() && !inCuttingCycle && (currentSystemState != STATE_RELOAD)) {
    if (!runCycleActive) {
      feedMotor->forceStop();
      feedMotorWasRunning = false;
      feedMotorTimeoutOccurred = false;
      feedMotorShouldRun = false;
      lastFeedMotorStateChange = 0;
      return;
    }
    
    if (!woodPresent) {
      feedMotor->forceStop();
      feedMotorWasRunning = false;
      feedMotorTimeoutOccurred = false;
      feedMotorShouldRun = false;
      lastFeedMotorStateChange = 0;
      return;
    }
  }
  
  // Feed motor timeout check
  if (feedMotor && feedMotor->isRunning() && !feedMotorTimeoutOccurred && (currentSystemState != STATE_RELOAD)) {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - feedMotorStartTime;
    
    if (elapsedTime >= feedMotorTimeout) {
      feedMotorTimeoutOccurred = true;
      setFeedMotorTimeoutLocked(true);
      
      feedMotor->forceStop();
      extendForwardClamp();
      feedMotorWasRunning = false;
    }
  }
  
  // Distance sensor trigger for cutting cycle
  if (distanceSensorTriggered && runCycleActive && woodPresent) {
    if (feedMotor && feedMotor->isRunning()) {
      feedMotor->forceStop();
      feedMotorTimeoutOccurred = false;
    }
    
    transitionToState(STATE_CUTTING);
    return;
  }
  
  // Reload switch monitoring
  bool reloadSwitchActive = isReloadSwitchActive();
  static bool reloadSwitchWasActive = false;
  
  if (reloadSwitchActive && !reloadSwitchWasActive) {
    if (feedMotor && feedMotor->isRunning()) {
      feedMotor->forceStop();
    }
    
    if (!isForwardClampRetracted()) {
      extendForwardClamp();
    }
    
    transitionToState(STATE_RELOAD);
    return;
  }
  
  reloadSwitchWasActive = reloadSwitchActive;
}

void exitIdleState() {
  // Motor enable/disable is handled by the target state
}

void resetIdleFeedMotorControl() {
  feedMotorShouldRun = false;
  feedMotorWasRunning = false;
  lastFeedMotorStateChange = 0;
  feedMotorTimeoutOccurred = false;
  resetFeedMotorTimeoutLock();
  
  clampShouldBeRetracted = false;
  clampWasRetracted = false;
  lastClampStateChange = 0;
  
  if (feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
  }
}
