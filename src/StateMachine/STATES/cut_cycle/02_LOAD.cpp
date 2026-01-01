#include "StateMachine.h"
#include "LOAD_State.h"
#include "Config.h"
#include "Pins_Definitions.h"

//╔═══╗ ════════════════════════════════════════════════════════════════ ╔═══╗
//║ ⚔️ LOAD STATE                                                        ║
//╚═══╝ ════════════════════════════════════════════════════════════════ ╚═══╝

//* ************************************************************************
//* ************************ CONFIGURATION ************************
//* ************************************************************************
static const int32_t AUTOMATIC_UNLOAD_STEPS = 5000;  // Steps to retract after wood sensor deactivation

//* ************************************************************************
//* ************************ STATE VARIABLES ************************
//* ************************************************************************
static unsigned long feedStartTime = 0;
static unsigned long sensorTriggerTime = 0;
static bool distanceSensorTriggered = false;
static bool delayTimerStarted = false;
static bool feedMotorRunning = false;
static bool loadExitCondition = false;
static bool timeoutOccurred = false;
static bool woodWasPresentAtStart = false;
static bool waitingForWoodReset = false;
static bool woodSensorDeactivated = false;

//* ************************************************************************
//* ************************ ENTER STATE ************************
//* ************************************************************************
void enterLoadState() {
  // Reset all state variables
  feedStartTime = 0;
  sensorTriggerTime = 0;
  distanceSensorTriggered = false;
  delayTimerStarted = false;
  feedMotorRunning = false;
  loadExitCondition = false;
  timeoutOccurred = false;
  woodWasPresentAtStart = false;
  waitingForWoodReset = false;
  woodSensorDeactivated = false;
  
  resetFeedMotorTimeoutLock();
  resetFeedMotorFlags();
  
  //! ************************************************************************
  //! STEP 1: VALIDATE PRE-CONDITIONS
  //! ************************************************************************
  if (!isRunCycleSwitchActive()) {
    loadExitCondition = true;
    return;
  }
  
  if (!isWoodPresent()) {
    loadExitCondition = true;
    return;
  }
  
  woodWasPresentAtStart = true;
  
  //! ************************************************************************
  //! STEP 2: PREPARE MOTORS AND CLAMP
  //! ************************************************************************
  if (!motorsEnabled) {
    enableAllMotors();
  }
  
  if (!isForwardClampRetracted()) {
    retractForwardClamp();
  }
  
  //! ************************************************************************
  //! STEP 3: VALIDATE MOTOR CONFIGURATION
  //! ************************************************************************
  if (feedMotorSpeed <= 0 || feedMotorAcceleration <= 0) {
    loadExitCondition = true;
    return;
  }
  
  if (!feedMotor) {
    Serial.println("ERROR: Feed motor not available");
    loadExitCondition = true;
    return;
  }
  
  //! ************************************************************************
  //! STEP 4: START FEED MOTOR
  //! ************************************************************************
  feedMotor->setSpeedInHz(feedMotorSpeed);
  feedMotor->setAcceleration(feedMotorAcceleration);
  feedMotor->runForward();
  
  feedStartTime = millis();
  feedMotorRunning = true;
  
  if (!feedMotor->isRunning()) {
    Serial.println("ERROR: Feed motor failed to start");
    loadExitCondition = true;
    return;
  }
}

//* ************************************************************************
//* ************************ UPDATE STATE ************************
//* ************************************************************************
void updateLoadState() {
  //! ************************************************************************
  //! SECTION 1: WOOD SENSOR DEACTIVATION HANDLING
  //! ************************************************************************
  // Priority: Handle wood sensor deactivation before other checks
  if (feedMotor && feedMotor->isRunning() && !distanceSensorTriggered && woodWasPresentAtStart) {
    if (!isWoodPresent() && !waitingForWoodReset) {
      handleWoodSensorDeactivation();
      return;
    }
  }
  
  //! ************************************************************************
  //! SECTION 2: EMERGENCY STOP CHECK
  //! ************************************************************************
  if (!isRunCycleSwitchActive()) {
    emergencyStopFeedOperation();
    return;
  }
  
  //! ************************************************************************
  //! SECTION 3: WAITING FOR WOOD RESET
  //! ************************************************************************
  if (waitingForWoodReset) {
    handleWoodResetWaiting();
    return;
  }
  
  //! ************************************************************************
  //! SECTION 4: TIMEOUT PROTECTION
  //! ************************************************************************
  if (feedMotorRunning && !timeoutOccurred) {
    if (millis() - feedStartTime >= feedMotorTimeout) {
      timeoutOccurred = true;
      emergencyStopFeedOperation();
      return;
    }
  }
  
  //! ************************************************************************
  //! SECTION 5: DISTANCE SENSOR DETECTION
  //! ************************************************************************
  if (!distanceSensorTriggered && isFeedDistanceSensorTriggered()) {
    distanceSensorTriggered = true;
    sensorTriggerTime = millis();
    
    if (feedMotor && feedMotor->isRunning()) {
      feedMotor->forceStop();
      feedMotorRunning = false;
    }
    
    extendForwardClamp();
    delayTimerStarted = true;
  }
  
  //! ************************************************************************
  //! SECTION 6: DELAY COMPLETION - TRANSITION TO CUTTING
  //! ************************************************************************
  if (delayTimerStarted && !loadExitCondition && !timeoutOccurred) {
    if (millis() - sensorTriggerTime >= woodDistanceDelay) {
      if (isRunCycleSwitchActive() && isWoodPresent()) {
        transitionToState(STATE_CUTTING);
        return;
      } else {
        loadExitCondition = true;
        emergencyStopFeedOperation();
        return;
      }
    }
  }
  
  //! ************************************************************************
  //! SECTION 7: MOTOR STATE VERIFICATION
  //! ************************************************************************
  if (!feedMotorRunning && feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
    
    if (feedMotor->isRunning()) {
      loadExitCondition = true;
      emergencyStopFeedOperation();
      return;
    }
  }
}

//* ************************************************************************
//* ************************ HELPER FUNCTIONS ************************
//* ************************************************************************

//! ************************************************************************
//! FUNCTION: Handle wood sensor deactivation with automatic unload
//! ************************************************************************
void handleWoodSensorDeactivation() {
  Serial.println("Wood sensor deactivated - feeding for extra " + String(woodSensorDeactivationDelay) + "ms");
  
  // Continue feeding for delay period while monitoring emergency stop
  unsigned long startRunoff = millis();
  while (millis() - startRunoff < woodSensorDeactivationDelay) {
    // Continuously restart motor to ensure it keeps running
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
    feedMotor->runForward();
    
    // Safety check for emergency stop
    updateRunCycleSwitch();
    if (!isRunCycleSwitchActive()) {
      feedMotor->forceStop();
      feedMotorRunning = false;
      emergencyStopFeedOperation();
      return;
    }
    
    delay(10);
  }
  
  // Stop motor
  if (feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
    feedMotorRunning = false;
  }
  
  extendForwardClamp();
  
  // Perform automatic unload
  if (feedMotor) {
    retractForwardClamp();
    
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
    feedMotor->move(-AUTOMATIC_UNLOAD_STEPS);
    
    while (feedMotor->isRunning()) {
      delay(10);
    }
    
    extendForwardClamp();
    
    waitingForWoodReset = true;
    woodSensorDeactivated = false;
  }
}

//! ************************************************************************
//! FUNCTION: Monitor wood sensor reset and restart feeding
//! ************************************************************************
void handleWoodResetWaiting() {
  if (!woodSensorDeactivated) {
    // Wait for sensor to fully deactivate
    if (!isWoodPresent()) {
      woodSensorDeactivated = true;
    }
  } else {
    // Wait for new wood to be detected
    if (isWoodPresent()) {
      waitingForWoodReset = false;
      woodSensorDeactivated = false;
      woodWasPresentAtStart = true;
      
      if (feedMotor) {
        feedMotor->setSpeedInHz(feedMotorSpeed);
        feedMotor->setAcceleration(feedMotorAcceleration);
        feedMotor->runForward();
        feedStartTime = millis();
        feedMotorRunning = true;
      } else {
        Serial.println("ERROR: Feed motor not available for restart");
      }
    }
  }
}

//* ************************************************************************
//* ************************ EMERGENCY STOP ************************
//* ************************************************************************
void emergencyStopFeedOperation() {
  if (feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
    feedMotorRunning = false;
  }
  
  extendForwardClamp();
  setFeedMotorTimeoutLocked(true);
  transitionToState(STATE_IDLE);
}

//* ************************************************************************
//* ************************ EXIT STATE ************************
//* ************************************************************************
void exitLoadState() {
  // Reset all state variables
  feedStartTime = 0;
  sensorTriggerTime = 0;
  distanceSensorTriggered = false;
  delayTimerStarted = false;
  feedMotorRunning = false;
  loadExitCondition = false;
  timeoutOccurred = false;
  woodWasPresentAtStart = false;
  waitingForWoodReset = false;
  woodSensorDeactivated = false;
  
  resetFeedDistanceSensor();
  
  if (timeoutOccurred) {
    timeoutOccurred = false;
  }
}
