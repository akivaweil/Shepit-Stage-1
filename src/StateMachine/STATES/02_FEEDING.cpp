#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ FEEDING STATE ********************************
//* ************************************************************************
// The FEEDING state handles the feed motor forward movement
// Motors remain enabled from the cutting state (no additional delay needed)

void enterFeedingState() {
  // Motors are permanently enabled - start feed movement immediately
  if (feedMotor) {
    Serial.println("Starting feed motor forward movement (" + String(feedMotorSteps) + " steps)");
    feedMotor->move(feedMotorSteps);
  }
}

void updateFeedingState() {
  // Motors are permanently enabled - no timeout management needed
  
  // Check if feed motor movement is complete
  if (feedMotor && !feedMotor->isRunning()) {
    Serial.println("Feed motor forward movement COMPLETE");
    
    // Feeding sequence complete, return to idle
    Serial.println("*** CUTTING CYCLE COMPLETE ***");
    transitionToState(STATE_IDLE);
  }
}

void exitFeedingState() {
  // Motors will be handled by the target state
  // If going to IDLE, motors will timeout after 2 seconds
} 