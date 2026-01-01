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

// Run cycle switch cycling requirement
static bool runCycleSwitchCycled = false;
static bool lastRunCycleSwitchState = false;

// Distance sensor edge detection - only trigger on rising edge
static bool lastDistanceSensorState = false;

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
  
  // Track run cycle switch state for cycling requirement
  runCycleSwitchCycled = false;
  lastRunCycleSwitchState = isRunCycleSwitchActive();
  
  // Reset distance sensor and edge detection when entering IDLE
  resetFeedDistanceSensor();
  lastDistanceSensorState = isWoodAtCorrectDistance();
  
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
    lastRunCycleActive = runCycleActive;
    lastWoodPresent = woodPresent;
  }
  
  // Feed motor runs when run cycle switch is ON and wood is present
  bool newFeedMotorShouldRun = runCycleActive && woodPresent && !inCuttingCycle && !isFeedMotorTimeoutLocked() && (currentSystemState != STATE_UNLOAD);
  
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
  feedMotorShouldRun = newFeedMotorShouldRun;
  
  // Check for cycle switch state changes
  static bool lastRunCycleState = false;
  if (runCycleActive != lastRunCycleState) {
    if (runCycleActive) {
      if (isFeedMotorTimeoutLocked()) {
        resetFeedMotorTimeoutLock();
        feedMotorTimeoutOccurred = false;
        wasLockedWhenSwitchOff = false;
      }
    } else {
      if (feedMotor && feedMotor->isRunning()) {
        feedMotor->forceStop();
        feedMotorTimeoutOccurred = false;
      }
      feedMotorShouldRun = false;
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
  
  // Detect when motor stops unexpectedly
  if (feedMotorWasRunning && feedMotor && !feedMotor->isRunning()) {
    feedMotorWasRunning = false;
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
          Serial.println("Starting feed motor forward");
          feedMotor->runForward();
          Serial.println("Feed motor runForward() called");
          delay(50); // Small delay to let motor start
          Serial.println("Feed motor isRunning check: " + String(feedMotor->isRunning() ? "TRUE" : "FALSE"));
          
          // Reset distance sensor state tracking when feed motor starts
          // This ensures we can detect rising edge when wood reaches sensor
          lastDistanceSensorState = isWoodAtCorrectDistance();
          
          // Set run cycle switch cycled flag when feed motor starts
          // This allows distance sensor to trigger cutting cycle
          runCycleSwitchCycled = true;
          
          if (currentSystemState != STATE_UNLOAD) {
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
  if (feedMotor && feedMotor->isRunning() && !inCuttingCycle && (currentSystemState != STATE_UNLOAD)) {
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
  if (feedMotor && feedMotor->isRunning() && !feedMotorTimeoutOccurred && (currentSystemState != STATE_UNLOAD)) {
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
  
  // Track run cycle switch cycling requirement
  bool currentRunCycleSwitchState = runCycleActive;
  if (currentRunCycleSwitchState != lastRunCycleSwitchState) {
    if (!lastRunCycleSwitchState && currentRunCycleSwitchState) {
      // Switch turned ON from OFF state - mark as cycled
      runCycleSwitchCycled = true;
    }
    lastRunCycleSwitchState = currentRunCycleSwitchState;
  }
  
  // Distance sensor trigger for cutting cycle
  // Only trigger on rising edge (transition from not triggered to triggered)
  // Only allow cutting cycle if run cycle switch has been cycled (off then on)
  bool distanceSensorRisingEdge = distanceSensorTriggered && !lastDistanceSensorState;
  if (distanceSensorRisingEdge && runCycleActive && woodPresent && runCycleSwitchCycled) {
    if (feedMotor && feedMotor->isRunning()) {
      feedMotor->forceStop();
      feedMotorTimeoutOccurred = false;
    }
    
    // Reset cycling flag after starting cutting cycle
    runCycleSwitchCycled = false;
    
    transitionToState(STATE_CUTTING);
    return;
  }
  
  // Update last distance sensor state for edge detection
  lastDistanceSensorState = distanceSensorTriggered;
  
  // Unload switch monitoring
  bool unloadSwitchActive = isUnloadSwitchActive();
  static bool unloadSwitchWasActive = false;
  
  if (unloadSwitchActive && !unloadSwitchWasActive) {
    if (feedMotor && feedMotor->isRunning()) {
      feedMotor->forceStop();
    }
    
    if (!isForwardClampRetracted()) {
      extendForwardClamp();
    }
    
    transitionToState(STATE_UNLOAD);
    return;
  }
  
  unloadSwitchWasActive = unloadSwitchActive;
}

void exitIdleState() {
  // Motor enable/disable is handled by the target state
}

void resetIdleFeedMotorControl(bool stopMotor) {
  feedMotorShouldRun = false;
  feedMotorWasRunning = false;
  lastFeedMotorStateChange = 0;
  feedMotorTimeoutOccurred = false;
  resetFeedMotorTimeoutLock();
  
  clampShouldBeRetracted = false;
  clampWasRetracted = false;
  lastClampStateChange = 0;
  
  if (stopMotor && feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
  }
}
