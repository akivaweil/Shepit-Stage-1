#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ RETURNING STATE ******************************
//* ************************************************************************
// The RETURNING state handles the cut motor backward movement with different settings
// Motors are permanently enabled - uses specific return speed and acceleration
// After return movement, checks conditions for next cutting cycle



void enterReturningState() {
  // Check only wood presence at the beginning of returning state
  // Run cycle switch is NOT checked here - once cutting cycle starts, it completes
  if (!isWoodPresent()) {
    Serial.println("RETURNING: NO WOOD DETECTED - Canceling cutting cycle");
    
    // Stop motors if they're running
    if (feedMotor && feedMotor->isRunning()) {
      feedMotor->forceStop();
    }
    if (cutMotor && cutMotor->isRunning()) {
      cutMotor->forceStop();
    }
    
    // Return to idle state
    transitionToState(STATE_IDLE);
    return;
  }
  
  // Wood present - proceed with returning state (regardless of run cycle switch)
  Serial.println("RETURNING: Wood detected - proceeding with returning state");
  Serial.println("RETURNING: NOTE: Cutting cycle continues regardless of run cycle switch state");
  
  // Motors are permanently enabled - configure motors for return movement
  
  // Retract clamp before any potential motor movement
  retractClamp();
  
  //! ************************************************************************
  //! STEP 1: START CUT MOTOR RETURN MOVEMENT
  //! ************************************************************************
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
  
  // Check if cut motor has completed its return movement
  bool cutMotorComplete = !cutMotor || !cutMotor->isRunning();
  
  if (cutMotorComplete) {
    Serial.println("Cut motor return movement COMPLETE");
    
    // Extend clamp now that feed motor movement is complete
    extendClamp();
    
    // Restore original cut motor settings for future cutting operations
    if (cutMotor) {
      cutMotor->setSpeedInHz(cutMotorSpeed);
      cutMotor->setAcceleration(cutMotorAcceleration);
    }
    
      // No feeding sequence needed - skip directly to condition checking
  Serial.println("No feeding sequence needed - checking conditions for next cycle");
  
  //! ************************************************************************
  //! CHECK CONDITIONS FOR CONTINUOUS CUTTING
  //! ************************************************************************
  // Check both wood presence AND run cycle switch status
  if (isWoodPresent() && isRunCycleSwitchActive()) {
    Serial.println("Wood still present + RUN CYCLE SWITCH ACTIVE - starting another cutting cycle");
    transitionToState(STATE_CUTTING);
  } else if (isWoodPresent() && !isRunCycleSwitchActive()) {
    Serial.println("Wood still present but RUN CYCLE SWITCH INACTIVE - returning to IDLE");
    transitionToState(STATE_IDLE);
  } else {
    Serial.println("*** CUTTING CYCLE COMPLETE - No more wood detected ***");
    transitionToState(STATE_IDLE);
  }
  }
  

}

void exitReturningState() {
  // Motors will be handled by the target state
  // If going to IDLE, motors will timeout after 2 seconds
} 