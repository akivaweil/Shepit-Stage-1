#include "StateMachine.h"
#include "UNLOAD_State.h"
#include "Config.h"
#include "Pins_Definitions.h"

// External motor objects from main.cpp
extern FastAccelStepper *feedMotor;
extern FastAccelStepper *cutMotor;

// Configuration
static unsigned long unloadStartTime = 0;
static bool unloadMotorMoving = false;
static bool autoUnloadMode = false; // Flag to indicate automatic unload from cutting state

void enterUnloadState() {
  if (isFeedMotorTimeoutLocked()) {
    resetFeedMotorTimeoutLock();
  }
  
  resetFeedMotorControlVariables(false);
  
  unloadStartTime = 0;
  unloadMotorMoving = false;
  
  // Check if entering from unload switch or automatic from cutting state
  bool unloadSwitchActive = digitalRead(UNLOAD_SWITCH_PIN) == HIGH;
  
  if (unloadSwitchActive) {
    // Manual unload via switch
    autoUnloadMode = false;
    enableAllMotors();
    retractForwardClamp();
    
    // Start continuous backward movement
    if (feedMotor) {
      feedMotor->setSpeedInHz(feedMotorSpeed);
      feedMotor->setAcceleration(feedMotorAcceleration);
      feedMotor->runBackward();
      unloadMotorMoving = true;
      unloadStartTime = millis();
    }
  } else {
    // Automatic unload triggered by wood absence during cutting
    autoUnloadMode = true;
    enableAllMotors();
    retractForwardClamp();
    
    // Start continuous backward movement
    if (feedMotor) {
      feedMotor->setSpeedInHz(feedMotorSpeed);
      feedMotor->setAcceleration(feedMotorAcceleration);
      feedMotor->runBackward();
      unloadMotorMoving = true;
      unloadStartTime = millis();
    }
  }
}

void updateUnloadState() {
  // Reset motor timeout to keep motors enabled during unload
  resetMotorTimeout();
  
  if (autoUnloadMode) {
    // Automatic unload mode: run for configured duration then transition to IDLE
    unsigned long elapsedTime = millis() - unloadStartTime;
    
    if (elapsedTime >= unloadDurationMs) {
      // Configured duration elapsed, stop motor and transition to IDLE
      if (feedMotor && feedMotor->isRunning()) {
        feedMotor->forceStop();
        unloadMotorMoving = false;
      }
      
      extendForwardClamp();
      transitionToState(STATE_IDLE);
      return;
    }
    
    // Keep motor running backward during the configured duration
    if (unloadMotorMoving && feedMotor && !feedMotor->isRunning()) {
      feedMotor->runBackward();
    }
  } else {
    // Manual unload mode: controlled by unload switch
    bool unloadSwitchActive = digitalRead(UNLOAD_SWITCH_PIN) == HIGH;
    
    if (!unloadSwitchActive) {
      if (feedMotor && feedMotor->isRunning()) {
        feedMotor->forceStop();
        unloadMotorMoving = false;
      }
      
      extendForwardClamp();
      transitionToState(STATE_IDLE);
      return;
    }
    
    // Keep motor running backward while switch is active
    if (unloadMotorMoving && feedMotor && !feedMotor->isRunning()) {
      feedMotor->runBackward();
    }
  }
}

void exitUnloadState() {
  unloadMotorMoving = false;
}

