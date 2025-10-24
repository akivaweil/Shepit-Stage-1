#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"
#include <Bounce2.h>

// External motor objects from main.cpp
extern FastAccelStepper *feedMotor;
extern FastAccelStepper *cutMotor;

//* ************************************************************************
//* ************************ HOMING STATE **********************************
//* ************************************************************************
// The HOMING state moves the cut motor backward until the home switch is triggered
// Once triggered, it positions the motor to the known home position
// Homing speed is slower than normal operation for accuracy

// Timing and state variables
static unsigned long homingStartTime = 0;
static bool homingMotorMoving = false;
static const unsigned long HOMING_TIMEOUT_MS = 30000; // 30 second timeout
static const float HOMING_SPEED = 1500.0; // Slower speed for homing accuracy
static const float HOMING_ACCELERATION = 10000.0; // Lower acceleration for homing

void enterHomingState() {
  // Reset homing variables
  homingStartTime = 0;
  homingMotorMoving = false;
  
  // Ensure motors are enabled
  enableAllMotors();
  
  Serial.println("HOMING: Starting homing sequence");
  
  // Check if already at home position
  if (isHomeSwitchTriggered()) {
    Serial.println("HOMING: Already at home position");
    // Reset motor position to 0
    if (cutMotor) {
      cutMotor->setCurrentPosition(0);
      Serial.println("HOMING: Cut motor position reset to 0");
    }
    // Return to idle
    transitionToState(STATE_IDLE);
    return;
  }
  
  // Start cut motor movement in reverse direction (toward home)
  if (cutMotor) {
    cutMotor->setSpeedInHz(HOMING_SPEED);
    cutMotor->setAcceleration(HOMING_ACCELERATION);
    
    // Move backward indefinitely until home switch triggers
    cutMotor->runBackward();
    homingMotorMoving = true;
    homingStartTime = millis();
    
    Serial.println("HOMING: Cut motor moving backward toward home position");
  } else {
    Serial.println("HOMING: ERROR - cutMotor pointer is NULL");
  }
}

void updateHomingState() {
  //! ************************************************************************
  //! TIMEOUT DETECTION
  //! ************************************************************************
  // Check for homing timeout (30 seconds)
  if (homingMotorMoving) {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - homingStartTime;
    
    if (elapsedTime >= HOMING_TIMEOUT_MS) {
      Serial.println("HOMING: TIMEOUT - homing took too long");
      
      // Stop the cut motor
      if (cutMotor && cutMotor->isRunning()) {
        cutMotor->forceStop();
        homingMotorMoving = false;
      }
      
      // Return to idle state
      transitionToState(STATE_IDLE);
      return;
    }
  }
  
  //! ************************************************************************
  //! HOME SWITCH DETECTION
  //! ************************************************************************
  // Check if home switch is triggered
  if (homingMotorMoving && isHomeSwitchTriggered()) {
    Serial.println("HOMING: Home switch triggered");
    
    // Stop the cut motor immediately
    if (cutMotor && cutMotor->isRunning()) {
      cutMotor->forceStop();
      delay(50); // Brief delay to ensure motor stops
      
      homingMotorMoving = false;
      
      // Reset motor position to 0 (home position)
      cutMotor->setCurrentPosition(0);
      Serial.println("HOMING: Cut motor position reset to 0 (home)");
    }
    
    // Homing complete - return to idle
    Serial.println("HOMING: Complete");
    transitionToState(STATE_IDLE);
    return;
  }
  
  // Continue running motor until home switch triggers or timeout occurs
}

void exitHomingState() {
  // Clean up state variables
  homingMotorMoving = false;
}

