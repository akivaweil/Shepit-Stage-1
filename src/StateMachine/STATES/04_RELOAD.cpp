#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

// External motor objects from main.cpp
extern FastAccelStepper *feedMotor;
extern FastAccelStepper *cutMotor;

// Configuration
static unsigned long reloadStartTime = 0;
static bool reloadMotorMoving = false;
static const int32_t RELOAD_STEPS = 5000;

void enterReloadState() {
  if (isFeedMotorTimeoutLocked()) {
    resetFeedMotorTimeoutLock();
  }
  
  resetFeedMotorControlVariables();
  
  reloadStartTime = 0;
  reloadMotorMoving = false;
  
  if (digitalRead(RELOAD_SWITCH_PIN) != HIGH) {
    transitionToState(STATE_IDLE);
    return;
  }
  
  enableAllMotors();
  
  retractForwardClamp();
  
  if (feedMotor && isCutMotorHomed()) {
    int32_t initialPosition = feedMotor->getCurrentPosition();
    
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
    feedMotor->move(-RELOAD_STEPS);
    reloadMotorMoving = true;
    reloadStartTime = millis();
  } else if (!isCutMotorHomed()) {
    Serial.println("=== RELOAD STATE BLOCKED ===");
    Serial.println("ERROR: Cut motor not homed - auto-homing before reload operation");
    Serial.println("Cut motor homed flag: " + String(isCutMotorHomed() ? "TRUE" : "FALSE"));
    Serial.println("Cut motor home switch: " + String(isCutMotorHomeSwitchTriggered() ? "TRIGGERED" : "NOT TRIGGERED"));
    Serial.println("Motors enabled: " + String(motorsEnabled ? "YES" : "NO"));
    Serial.println("Setting return state to RELOAD and transitioning to HOMING...");
    setReturnStateAfterHoming(STATE_RELOAD);
    transitionToState(STATE_HOMING);
    return;
  }
}

void updateReloadState() {
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
  
  // Check for movement completion
  if (reloadMotorMoving && feedMotor && !feedMotor->isRunning()) {
    reloadMotorMoving = false;
    extendForwardClamp();
    transitionToState(STATE_IDLE);
  }
}

void exitReloadState() {
  reloadMotorMoving = false;
}
