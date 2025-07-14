#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ RETURNING STATE ******************************
//* ************************************************************************
// The RETURNING state handles the cut motor backward movement with different settings
// Motors are permanently enabled - uses specific return speed and acceleration

void enterReturningState() {
  // Motors are permanently enabled - configure cut motor for return movement
  if (cutMotor) {
    // Set return-specific speed and acceleration
    cutMotor->setSpeedInHz(cutMotorReturnSpeed);
    cutMotor->setAcceleration(cutMotorReturnAcceleration);
    
    // Start cut motor backward movement immediately
    Serial.println("Starting cut motor return movement (" + String(cutMotorSteps) + " steps)");
    cutMotor->move(-cutMotorSteps);
  }
}

void updateReturningState() {
  // Motors are permanently enabled - no timeout management needed
  
  // Check if cut motor return movement is complete
  if (cutMotor && !cutMotor->isRunning()) {
    Serial.println("Cut motor return movement COMPLETE");
    
    // Restore original cut motor settings for future cutting operations
    cutMotor->setSpeedInHz(cutMotorSpeed);
    cutMotor->setAcceleration(cutMotorAcceleration);
    
    // Return movement complete, transition to feeding
    transitionToState(STATE_FEEDING);
  }
}

void exitReturningState() {
  // Motors stay enabled for the feeding state
  // Motor settings are restored in updateReturningState() before transition
} 