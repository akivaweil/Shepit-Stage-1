#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ CUTTING STATE ********************************
//* ************************************************************************
// The CUTTING state handles the cut motor forward and backward movements
// Motors are enabled with delay to ensure proper wake-up from sleep mode

void enterCuttingState() {
  // Enable motors with delay to ensure proper wake-up from sleep mode
  enableAllMotorsWithDelay();
}

void updateCuttingState() {
  // Wait for motor enable delay to complete before starting movement
  if (!isMotorEnableDelayComplete()) {
    return; // Still waiting for motor stabilization
  }
  
  // Start cut motor movement if not already running
  if (cutMotor && !cutMotor->isRunning()) {
    // Check if this is the initial movement start
    if (cutMotor->getCurrentPosition() == cutMotor->targetPos()) {
      // Motor hasn't started moving yet, start the movement
      Serial.println("Starting cut motor forward movement (" + String(cutMotorSteps) + " steps)");
      cutMotor->move(cutMotorSteps);
    } else {
      // Movement was already in progress and now complete
      Serial.println("Cut motor forward movement COMPLETE");
      
      // Cutting sequence complete, transition to returning
      transitionToState(STATE_RETURNING);
    }
  }
}

void exitCuttingState() {
  // Motors stay enabled for the next state
  // No need to disable motors here
} 