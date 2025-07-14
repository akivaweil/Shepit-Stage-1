#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ CUTTING STATE ********************************
//* ************************************************************************
// The CUTTING state handles the cut motor forward and backward movements
// Motors are enabled with delay and stay enabled throughout the entire sequence

void enterCuttingState() {
  // Motors are permanently enabled - start cut motor forward movement immediately
  if (cutMotor) {
    Serial.println("Starting cut motor forward movement (" + String(cutMotorSteps) + " steps)");
    cutMotor->move(cutMotorSteps);
  }
}

void updateCuttingState() {
  // Motors are permanently enabled - no timeout management needed
  
  // Check if cut motor forward movement is complete
  if (cutMotor && !cutMotor->isRunning()) {
    Serial.println("Cut motor forward movement COMPLETE");
    
    // Cutting sequence complete, transition to returning
    transitionToState(STATE_RETURNING);
  }
}

void exitCuttingState() {
  // Motors stay enabled for the feeding state
  // No need to disable motors here
} 