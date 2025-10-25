#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ FEED TO DISTANCE STATE ************************
//* ************************************************************************

// State-specific variables
static unsigned long feedStartTime = 0;
static unsigned long sensorTriggerTime = 0;
static bool distanceSensorTriggered = false;
static bool delayTimerStarted = false;
static bool feedMotorRunning = false;
static bool feedToDistanceExitCondition = false;
static bool timeoutOccurred = false;
static bool woodWasPresentAtStart = false;
static bool waitingForWoodReset = false;
static bool woodSensorDeactivated = false;

// Automatic reload constants
static const int32_t AUTOMATIC_RELOAD_STEPS = 5000;

void enterFeedToDistanceState() {
  feedStartTime = 0;
  sensorTriggerTime = 0;
  distanceSensorTriggered = false;
  delayTimerStarted = false;
  feedMotorRunning = false;
  feedToDistanceExitCondition = false;
  timeoutOccurred = false;
  woodWasPresentAtStart = false;
  waitingForWoodReset = false;
  woodSensorDeactivated = false;
  
  resetFeedMotorTimeoutLock();
  resetFeedMotorFlags();
  
  if (!isRunCycleSwitchActive()) {
    feedToDistanceExitCondition = true;
    return;
  }
  
  if (!isWoodPresent()) {
    feedToDistanceExitCondition = true;
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
    feedToDistanceExitCondition = true;
    return;
  }
  
  if (feedMotor && isCutMotorHomed()) {
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
    feedMotor->runForward();
    
    feedStartTime = millis();
    feedMotorRunning = true;
    
    if (!feedMotor->isRunning()) {
      feedToDistanceExitCondition = true;
      return;
    }
  } else if (!isCutMotorHomed()) {
    Serial.println("ERROR: Cut motor not homed - cannot start feed operation");
    feedToDistanceExitCondition = true;
    return;
  } else {
    feedToDistanceExitCondition = true;
    return;
  }
}

void updateFeedToDistanceState() {
  if (!isRunCycleSwitchActive()) {
    emergencyStopFeedOperation();
    return;
  }
  
  // Monitor wood presence during feeding
  if (feedMotorRunning && woodWasPresentAtStart && !distanceSensorTriggered) {
    if (!isWoodPresent()) {
      if (feedMotor && feedMotor->isRunning()) {
        feedMotor->forceStop();
        feedMotorRunning = false;
      }
      
      extendForwardClamp();
      
      if (feedMotor) {
        retractForwardClamp();
        
        feedMotor->setSpeedInHz(feedMotorSpeed);
        feedMotor->setAcceleration(feedMotorAcceleration);
        feedMotor->move(-AUTOMATIC_RELOAD_STEPS);
        
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
        
        if (feedMotor && isCutMotorHomed()) {
          feedMotor->setSpeedInHz(feedMotorSpeed);
          feedMotor->setAcceleration(feedMotorAcceleration);
          feedMotor->runForward();
          feedStartTime = millis();
          feedMotorRunning = true;
        } else if (!isCutMotorHomed()) {
          Serial.println("ERROR: Cut motor not homed - cannot restart feed operation");
          feedToDistanceExitCondition = true;
          return;
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
  if (delayTimerStarted && !feedToDistanceExitCondition && !timeoutOccurred) {
    if (millis() - sensorTriggerTime >= woodDistanceDelay) {
      if (isRunCycleSwitchActive() && isWoodPresent()) {
        transitionToState(STATE_CUTTING);
        return;
      } else {
        feedToDistanceExitCondition = true;
        emergencyStopFeedOperation();
        return;
      }
    }
  }
  
  // Motor stop verification
  if (!feedMotorRunning && feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
    
    if (feedMotor->isRunning()) {
      feedToDistanceExitCondition = true;
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

void exitFeedToDistanceState() {
  feedStartTime = 0;
  sensorTriggerTime = 0;
  distanceSensorTriggered = false;
  delayTimerStarted = false;
  feedMotorRunning = false;
  feedToDistanceExitCondition = false;
  timeoutOccurred = false;
  woodWasPresentAtStart = false;
  waitingForWoodReset = false;
  woodSensorDeactivated = false;
  
  resetFeedDistanceSensor();
  
  if (timeoutOccurred) {
    timeoutOccurred = false;
  }
}
