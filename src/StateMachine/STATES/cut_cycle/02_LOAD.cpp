#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ LOAD STATE ************************
//* ************************************************************************

// State-specific variables
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

// Automatic unload constants
static const int32_t AUTOMATIC_UNLOAD_STEPS = 5000;

void enterLoadState() {
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
  
  if (!isRunCycleSwitchActive()) {
    loadExitCondition = true;
    return;
  }
  
  if (!isWoodPresent()) {
    loadExitCondition = true;
    return;
  }
  
  woodWasPresentAtStart = true;
  
  if (!motorsEnabled) {
    enableAllMotors();
  }
  
  if (!isForwardClampRetracted()) {
    retractForwardClamp();
  }
  
  if (feedMotorSpeed <= 0 || feedMotorAcceleration <= 0) {
    loadExitCondition = true;
    return;
  }
  
  if (!feedMotor) {
    Serial.println("ERROR: Feed motor not available");
    loadExitCondition = true;
    return;
  }
  
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

void updateLoadState() {
  // CRITICAL: Check wood sensor deactivation FIRST before ANY other logic
  // Check ONLY motor state and wood sensor - ignore all flags
  if (feedMotor && feedMotor->isRunning() && !distanceSensorTriggered && woodWasPresentAtStart) {
    if (!isWoodPresent() && !waitingForWoodReset) {
      Serial.println("Wood sensor deactivated - feeding for extra " + String(woodSensorDeactivationDelay) + "ms");
      
      // BLOCKING DELAY - ABSOLUTELY GUARANTEE motor runs for full duration
      unsigned long startRunoff = millis();
      while (millis() - startRunoff < woodSensorDeactivationDelay) {
          // FORCE motor to keep running - check every loop iteration
          if (!feedMotor->isRunning()) {
             feedMotor->setSpeedInHz(feedMotorSpeed);
             feedMotor->setAcceleration(feedMotorAcceleration);
             feedMotor->runForward();
          }
          
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
      
      // NOW stop motor and unload
      if (feedMotor && feedMotor->isRunning()) {
        feedMotor->forceStop();
        feedMotorRunning = false;
      }
      
      extendForwardClamp();
      
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
      
      return;
    }
  }
  
  // NOW check run cycle switch AFTER wood sensor check
  if (!isRunCycleSwitchActive()) {
    emergencyStopFeedOperation();
    return;
  }
  
  // Monitor for wood sensor reset
  if (waitingForWoodReset) {
    if (!woodSensorDeactivated) {
      if (!isWoodPresent()) {
        woodSensorDeactivated = true;
      }
    } else {
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
    
    return;
  }
  
  // Timeout protection
  if (feedMotorRunning && !timeoutOccurred) {
    if (millis() - feedStartTime >= feedMotorTimeout) {
      timeoutOccurred = true;
      emergencyStopFeedOperation();
      return;
    }
  }
  
  // Distance sensor detection
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
  
  // Delay completion processing
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
  
  // Motor stop verification
  if (!feedMotorRunning && feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
    
    if (feedMotor->isRunning()) {
      loadExitCondition = true;
      emergencyStopFeedOperation();
      return;
    }
  }
}

void emergencyStopFeedOperation() {
  if (feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
    feedMotorRunning = false;
  }
  
  extendForwardClamp();
  setFeedMotorTimeoutLocked(true);
  transitionToState(STATE_IDLE);
}

void exitLoadState() {
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
