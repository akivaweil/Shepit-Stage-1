#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ CUTTING STATE ********************************
//* ************************************************************************
// The CUTTING state handles the cut motor forward movement
// Motors are enabled with delay to ensure proper wake-up from sleep mode

static bool cutMotorStarted = false;

void enterCuttingState() {
  // Enable motors with delay to ensure proper wake-up from sleep mode
  enableAllMotorsWithDelay();
  
  // Reset movement tracking flag
  cutMotorStarted = false;
}

void updateCuttingState() {
  // Wait for motor enable delay to complete before starting movement
  if (!isMotorEnableDelayComplete()) {
    return; // Still waiting for motor stabilization
  }
  
  // Start cut motor movement if not already started
  if (!cutMotorStarted && cutMotor) {
    Serial.println("Starting cut motor forward movement (" + String(cutMotorSteps) + " steps)");
    cutMotor->move(cutMotorSteps);
    cutMotorStarted = true;
  }
  
  // Check if movement is complete
  if (cutMotorStarted && cutMotor && !cutMotor->isRunning()) {
    Serial.println("Cut motor forward movement COMPLETE");
    
    // Cutting sequence complete, transition to returning
    transitionToState(STATE_RETURNING);
  }
}

void exitCuttingState() {
  // Reset movement tracking flag for next cycle
  cutMotorStarted = false;
  
  // Motors stay enabled for the next state
  // No need to disable motors here
} 