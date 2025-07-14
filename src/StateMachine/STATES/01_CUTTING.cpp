#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ CUTTING STATE ********************************
//* ************************************************************************
// The CUTTING state handles the cut motor forward and backward movements
// Motors are enabled with delay and stay enabled throughout the entire sequence

void enterCuttingState() {
  // Motors are permanently enabled - no need to enable them
  
  // Reset cutting phase to forward
  currentCuttingPhase = CUT_FORWARD_PHASE;
  
  // Start the cut motor forward movement immediately
  if (cutMotor) {
    Serial.println("Starting cut motor forward movement (" + String(cutMotorSteps) + " steps)");
    cutMotor->move(cutMotorSteps);
  }
}

void updateCuttingState() {
  // Motors are permanently enabled - no timeout management needed
  
  // Handle cutting phases
  switch (currentCuttingPhase) {
    case CUT_FORWARD_PHASE:
      // Check if cut motor forward movement is complete
      if (cutMotor && !cutMotor->isRunning()) {
        Serial.println("Cut motor forward movement COMPLETE");
        
        // Move to backward phase
        currentCuttingPhase = CUT_BACKWARD_PHASE;
        
        // Start cut motor backward movement
        Serial.println("Starting cut motor backward movement (" + String(cutMotorSteps) + " steps)");
        cutMotor->move(-cutMotorSteps);
      }
      break;
      
    case CUT_BACKWARD_PHASE:
      // Check if cut motor backward movement is complete
      if (cutMotor && !cutMotor->isRunning()) {
        Serial.println("Cut motor backward movement COMPLETE");
        
        // Cutting sequence complete, transition to feeding
        transitionToState(STATE_FEEDING);
      }
      break;
  }
}

void exitCuttingState() {
  // Motors stay enabled for the feeding state
  // No need to disable motors here
} 