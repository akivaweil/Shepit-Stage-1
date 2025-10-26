#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

// External motor objects from main.cpp
extern FastAccelStepper *cutMotor;

//* ************************************************************************
//* ************************ HOMING STATE **********************************
//* ************************************************************************

// Configuration
static const float HOMING_SPEED = 400.0;
static const float HOMING_ACCELERATION = 10000.0;

// State tracking
static bool homingMotorMoving = false;

void enterHomingState() {
  homingMotorMoving = false;
  
  Serial.println("=== ENTERING HOMING STATE ===");
  Serial.println("Motors enabled status: " + String(motorsEnabled ? "ENABLED" : "DISABLED"));
  Serial.println("Enabling all motors for homing...");
  enableAllMotors();
  Serial.println("Motors enabled status after enableAllMotors(): " + String(motorsEnabled ? "ENABLED" : "DISABLED"));
  
  Serial.println("Cut motor home switch status: " + String(isCutMotorHomeSwitchTriggered() ? "TRIGGERED" : "NOT TRIGGERED"));
  Serial.println("Cut motor homed flag: " + String(isCutMotorHomed() ? "TRUE" : "FALSE"));
  
  // Always start cut motor moving backward toward cut motor home switch
  // This ensures the cut motor home switch is pressed before proceeding
  if (cutMotor) {
    Serial.println("Cut motor pointer valid - setting speed and acceleration");
    cutMotor->setSpeedInHz(HOMING_SPEED);
    cutMotor->setAcceleration(HOMING_ACCELERATION);
    Serial.println("Starting cut motor movement backward...");
    cutMotor->move(-1000000);
    homingMotorMoving = true;
    Serial.println("Cut motor started moving backward at " + String(HOMING_SPEED) + " Hz");
    Serial.println("Cut motor running status: " + String(cutMotor->isRunning() ? "RUNNING" : "NOT RUNNING"));
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
        Serial.println("Cut motor stopped and position set to 0");
      }
      homingMotorMoving = false;
      setCutMotorHomed(true); // Mark cut motor as homed for safety
      Serial.println("Cut motor homed flag set to TRUE");
      SystemState returnState = getReturnStateAfterHoming();
      Serial.println("Return state after homing: " + String(returnState));
      Serial.println("Transitioning to return state...");
      transitionToState(returnState);
    }
  } else {
    // Add periodic status update if motor should be moving but isn't
    static unsigned long lastStatusLog = 0;
    if (millis() - lastStatusLog > 3000) {
      lastStatusLog = millis();
      Serial.println("HOMING WAITING: Motor not moving - check switch status");
      Serial.println("  Home switch: " + String(isCutMotorHomeSwitchTriggered() ? "TRIGGERED" : "NOT TRIGGERED"));
      Serial.println("  Motors enabled: " + String(motorsEnabled ? "YES" : "NO"));
    }
  }
}

void exitHomingState() {
  homingMotorMoving = false;
}

