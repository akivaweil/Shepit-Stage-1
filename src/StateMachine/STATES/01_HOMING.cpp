#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

// External motor objects from main.cpp
extern FastAccelStepper *cutMotor;

//* ************************************************************************
//* ************************ HOMING STATE **********************************
//* ************************************************************************

// Configuration
static const float HOMING_SPEED = 200.0;
static const float HOMING_ACCELERATION = 10000.0;

// State tracking
static bool homingMotorMoving = false;

void enterHomingState() {
  homingMotorMoving = false;
  enableAllMotors();
  
  // Check if already at home position
  if (isHomeSwitchTriggered()) {
    if (cutMotor) {
      cutMotor->setCurrentPosition(0);
    }
    transitionToState(STATE_FEED_TO_DISTANCE);
    return;
  }
  
  // Start cut motor moving backward toward home switch
  if (cutMotor) {
    cutMotor->setSpeedInHz(HOMING_SPEED);
    cutMotor->setAcceleration(HOMING_ACCELERATION);
    cutMotor->move(-1000000);
    homingMotorMoving = true;
  }
}

void updateHomingState() {
  //! ************************************************************************
  //! CHECK FOR HOME SWITCH TRIGGER
  //! ************************************************************************
  if (homingMotorMoving && isHomeSwitchTriggered()) {
    if (cutMotor && cutMotor->isRunning()) {
      cutMotor->forceStop();
      cutMotor->setCurrentPosition(0);
    }
    homingMotorMoving = false;
    transitionToState(STATE_FEED_TO_DISTANCE);
  }
}

void exitHomingState() {
  homingMotorMoving = false;
}

