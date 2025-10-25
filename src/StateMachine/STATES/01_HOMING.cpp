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
  
  Serial.println("Starting cut motor homing sequence");
  Serial.println("Cut motor home switch status: " + String(isCutMotorHomeSwitchTriggered() ? "TRIGGERED" : "NOT TRIGGERED"));
  
  // Always start cut motor moving backward toward cut motor home switch
  // This ensures the cut motor home switch is pressed before proceeding
  if (cutMotor) {
    cutMotor->setSpeedInHz(HOMING_SPEED);
    cutMotor->setAcceleration(HOMING_ACCELERATION);
    cutMotor->move(-1000000);
    homingMotorMoving = true;
    Serial.println("Cut motor started moving backward at " + String(HOMING_SPEED) + " Hz");
  } else {
    Serial.println("ERROR: Cut motor not available for homing");
  }
}

void updateHomingState() {
  //! ************************************************************************
  //! CHECK FOR CUT MOTOR HOME SWITCH TRIGGER
  //! ************************************************************************
  if (homingMotorMoving) {
    // Check cut motor home switch more aggressively
    if (isCutMotorHomeSwitchTriggered()) {
      Serial.println("Cut motor home switch triggered - stopping cut motor");
      if (cutMotor && cutMotor->isRunning()) {
        cutMotor->forceStop();
        // Wait a moment to ensure motor stops
        delay(10);
        cutMotor->setCurrentPosition(0);
      }
      homingMotorMoving = false;
      setCutMotorHomed(true); // Mark cut motor as homed for safety
      Serial.println("Cut motor homed - transitioning to return state");
      transitionToState(getReturnStateAfterHoming());
    }
  }
}

void exitHomingState() {
  homingMotorMoving = false;
}

