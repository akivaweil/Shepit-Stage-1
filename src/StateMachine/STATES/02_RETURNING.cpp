#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ RETURNING STATE ******************************
//* ************************************************************************
// The RETURNING state handles the cut motor backward movement with different settings
// Motors are permanently enabled - uses specific return speed and acceleration

void enterReturningState() {
  // Motors are permanently enabled - configure motors for return movement
  
  //! ************************************************************************
  //! STEP 1: CONFIGURE AND START FEED MOTOR PULLBACK (SIMULTANEOUS)
  //! ************************************************************************
  if (feedMotor) {
    // Use existing feed motor settings for pullback movement
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
    
    // Start feed motor pullback movement (negative direction)
    Serial.println("Starting feed motor pullback (" + String(feedMotorPullbackSteps) + " steps)");
    feedMotor->move(-feedMotorPullbackSteps);
  }
  
  //! ************************************************************************
  //! STEP 2: CONFIGURE AND START CUT MOTOR RETURN (SIMULTANEOUS)
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
  
  // Check if both motors have completed their movements
  bool feedMotorComplete = !feedMotor || !feedMotor->isRunning();
  bool cutMotorComplete = !cutMotor || !cutMotor->isRunning();
  
  if (feedMotorComplete && cutMotorComplete) {
    Serial.println("Feed motor pullback COMPLETE");
    Serial.println("Cut motor return movement COMPLETE");
    
    // Restore original cut motor settings for future cutting operations
    if (cutMotor) {
      cutMotor->setSpeedInHz(cutMotorSpeed);
      cutMotor->setAcceleration(cutMotorAcceleration);
    }
    
    // Both movements complete, transition to feeding
    transitionToState(STATE_FEEDING);
  }
}

void exitReturningState() {
  // Motors stay enabled for the feeding state
  // Motor settings are restored in updateReturningState() before transition
} 