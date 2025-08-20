#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ FEED TO DISTANCE STATE ************************
//* ************************************************************************
// This state feeds wood forward until the distance sensor is triggered,
// then waits for a configured delay before transitioning to the cutting cycle.
// Includes comprehensive safety monitoring and timeout protection.

//* ************************************************************************
//* ************************ STATE VARIABLES *******************************
//* ************************************************************************

// State-specific variables
static unsigned long feedStartTime = 0;           // When feed motor started
static unsigned long sensorTriggerTime = 0;       // When distance sensor triggered
static bool distanceSensorTriggered = false;      // Distance sensor state flag
static bool delayTimerStarted = false;            // Delay timer state flag
static bool feedMotorRunning = false;             // Feed motor running state
static bool safetyViolationDetected = false;      // Safety violation flag
static bool timeoutOccurred = false;              // Timeout flag


// Safety constants
const unsigned long FEED_TIMEOUT_MS = 3000;       // 2-second safety limit

//* ************************************************************************
//* ************************ STATE ENTRY FUNCTION **************************
//* ************************************************************************

void enterFeedToDistanceState() {
  //! ************************************************************************
  //! STEP 1: INITIALIZE STATE VARIABLES
  //! ************************************************************************
  
  // Reset all sequence variables for clean state
  feedStartTime = 0;
  sensorTriggerTime = 0;
  distanceSensorTriggered = false;
  delayTimerStarted = false;
  feedMotorRunning = false;
  safetyViolationDetected = false;
  timeoutOccurred = false;
  
  // Clear feed motor timeout locks from previous states
  resetFeedMotorTimeoutLock();
  
  // Reset feed motor control variables
  resetFeedMotorFlags();
  
  //! ************************************************************************
  //! STEP 2: PERFORM SAFETY CHECKS
  //! ************************************************************************
  
  // Verify run cycle switch is still active
  if (!isRunCycleSwitchActive()) {
    safetyViolationDetected = true;
    Serial.println("SAFETY VIOLATION: Run cycle switch deactivated during entry");
    return;
  }
  
  // Ensure all motors are enabled
  if (!motorsEnabled) {
    enableAllMotors();
    Serial.println("Motors enabled for feed operation");
  }
  
  // Retract clamp to allow feed motor movement
  if (!isClampRetracted()) {
    retractClamp();
    Serial.println("Clamp retracted for feed motor movement");
  }
  
  //! ************************************************************************
  //! STEP 3: CONFIGURE AND START FEED MOTOR
  //! ************************************************************************
  
  // Validate feed motor speed and acceleration values
  if (feedMotorSpeed <= 0 || feedMotorAcceleration <= 0) {
    safetyViolationDetected = true;
    Serial.println("SAFETY VIOLATION: Invalid motor parameters");
    return;
  }
  
  // Start continuous forward movement
  if (feedMotor) {
    // Set motor parameters
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
    
    // Start forward movement
    feedMotor->runForward();
    
    // Record start time and set movement flags
    feedStartTime = millis();
    feedMotorRunning = true;
    
    // Verify motor is actually running
    if (!feedMotor->isRunning()) {
      safetyViolationDetected = true;
      Serial.println("SAFETY VIOLATION: Feed motor failed to start");
      return;
    }
    
    Serial.println("Feed motor started - moving forward at " + String(feedMotorSpeed) + " Hz");
  } else {
    safetyViolationDetected = true;
    Serial.println("SAFETY VIOLATION: Feed motor object not available");
    return;
  }
}

//* ************************************************************************
//* ************************ STATE UPDATE FUNCTION *************************
//* ************************************************************************

void updateFeedToDistanceState() {
  //! ************************************************************************
  //! STEP 1: SAFETY MONITORING
  //! ************************************************************************
  
  // Check if run cycle switch is deactivated during operation
  if (!isRunCycleSwitchActive()) {
    Serial.println("SAFETY VIOLATION: Run cycle switch deactivated - stopping immediately");
    emergencyStopFeedOperation();
    return;
  }
  
  //! ************************************************************************
  //! STEP 2: TIMEOUT PROTECTION
  //! ************************************************************************
  
  // Check 2-second safety limit
  if (feedMotorRunning && !timeoutOccurred) {
    if (millis() - feedStartTime >= FEED_TIMEOUT_MS) {
      timeoutOccurred = true;
      Serial.println("SAFETY TIMEOUT: Feed motor ran for 2 seconds without sensor trigger");
      emergencyStopFeedOperation();
      return;
    }
  }
  
  //! ************************************************************************
  //! STEP 3: DISTANCE SENSOR DETECTION
  //! ************************************************************************
  
  // Check if distance sensor is triggered
  if (!distanceSensorTriggered && isFeedDistanceSensorTriggered()) {
    distanceSensorTriggered = true;
    sensorTriggerTime = millis();
    
    Serial.println("Distance sensor triggered - stopping feed motor");
    
    // Stop the feed motor immediately
    if (feedMotor && feedMotor->isRunning()) {
      feedMotor->forceStop();
      feedMotorRunning = false;
      
      // Verify motor actually stopped
      if (feedMotor->isRunning()) {
        Serial.println("WARNING: Feed motor did not stop on first attempt");
        // Will be handled by motor stop verification below
      }
    }
    
    // Extend clamp to secure wood
    extendClamp();
    Serial.println("Clamp extended to secure wood");
    
    // Start the configured delay timer
    delayTimerStarted = true;
    Serial.println("Delay timer started - waiting " + String(woodDistanceDelay) + "ms");
  }
  
  //! ************************************************************************
  //! STEP 4: DELAY COMPLETION PROCESSING
  //! ************************************************************************
  
  // Process delay completion
  if (delayTimerStarted && !safetyViolationDetected && !timeoutOccurred) {
    if (millis() - sensorTriggerTime >= woodDistanceDelay) {
      // Check if both conditions are met for cutting cycle
      if (isRunCycleSwitchActive() && isWoodPresent()) {
        Serial.println("Delay complete - conditions verified - transitioning to CUTTING state");
        transitionToState(STATE_CUTTING);
        return;
      } else {
        // Conditions failed - return to IDLE
        if (!isRunCycleSwitchActive()) {
          Serial.println("SAFETY VIOLATION: Run cycle switch deactivated during delay");
        } else {
          Serial.println("SAFETY VIOLATION: Wood no longer present during delay");
        }
        safetyViolationDetected = true;
        emergencyStopFeedOperation();
        return;
      }
    }
  }
  
  //! ************************************************************************
  //! STEP 5: MOTOR STOP VERIFICATION
  //! ************************************************************************
  
  // Verify motors actually stopped if stop was commanded
  if (!feedMotorRunning && feedMotor && feedMotor->isRunning()) {
    // Motor is still running despite stop command - use force stop
    Serial.println("WARNING: Feed motor still running - using force stop");
    feedMotor->forceStop();
    
    // If still running after force stop, this is a critical failure
    if (feedMotor->isRunning()) {
      Serial.println("CRITICAL: Feed motor failed to stop even with force stop");
      safetyViolationDetected = true;
      emergencyStopFeedOperation();
      return;
    }
  }
}

//* ************************************************************************
//* ************************ EMERGENCY STOP FUNCTION ***********************
//* ************************************************************************

void emergencyStopFeedOperation() {
  // Stop feed motor immediately
  if (feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
    feedMotorRunning = false;
    Serial.println("Emergency stop: Feed motor stopped");
  }
  
  // Extend clamp to secure wood
  extendClamp();
  Serial.println("Emergency stop: Clamp extended to secure wood");
  
  // Set feed motor timeout lock
  setFeedMotorTimeoutLocked(true);
  
  // Return to IDLE state
  Serial.println("Emergency stop: Returning to IDLE state");
  transitionToState(STATE_IDLE);
}

//* ************************************************************************
//* ************************ STATE EXIT FUNCTION ***************************
//* ************************************************************************

void exitFeedToDistanceState() {
  //! ************************************************************************
  //! STEP 1: CLEANUP PROCESS
  //! ************************************************************************
  
  // Reset state-specific variables
  feedStartTime = 0;
  sensorTriggerTime = 0;
  distanceSensorTriggered = false;
  delayTimerStarted = false;
  feedMotorRunning = false;
  safetyViolationDetected = false;
  timeoutOccurred = false;
  
  // Clear sensor trigger flags
  resetFeedDistanceSensor();
  
  // Remove timeout flags
  if (timeoutOccurred) {
    timeoutOccurred = false;
  }
  
  // Log exit state information
  Serial.println("Exiting FEED_TO_DISTANCE state - cleanup complete");
}
