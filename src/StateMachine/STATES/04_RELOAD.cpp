#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

// External motor objects from main.cpp
extern FastAccelStepper *feedMotor;
extern FastAccelStepper *cutMotor;

// Configuration
static unsigned long reloadStartTime = 0;
static bool reloadMotorMoving = false;

void enterReloadState() {
  if (isFeedMotorTimeoutLocked()) {
    resetFeedMotorTimeoutLock();
  }
  
  resetFeedMotorControlVariables(false);
  
  reloadStartTime = 0;
  reloadMotorMoving = false;
  
  if (digitalRead(RELOAD_SWITCH_PIN) != HIGH) {
    transitionToState(STATE_IDLE);
    return;
  }
  
  enableAllMotors();
  
  retractForwardClamp();
  
  // Start continuous backward movement
  if (feedMotor) {
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
    feedMotor->runBackward();
    reloadMotorMoving = true;
    reloadStartTime = millis();
  }
}

void updateReloadState() {
  // Reset motor timeout to keep motors enabled during reload
  resetMotorTimeout();
  
  // Check if reload switch is turned off
  bool reloadSwitchActive = digitalRead(RELOAD_SWITCH_PIN) == HIGH;
  
  if (!reloadSwitchActive) {
    if (feedMotor && feedMotor->isRunning()) {
      feedMotor->forceStop();
      reloadMotorMoving = false;
    }
    
    extendForwardClamp();
    transitionToState(STATE_IDLE);
    return;
  }
  
  // Keep motor running backward while switch is active
  if (reloadMotorMoving && feedMotor && !feedMotor->isRunning()) {
    feedMotor->runBackward();
  }
}

void exitReloadState() {
  reloadMotorMoving = false;
}
