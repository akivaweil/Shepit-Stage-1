#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ FEEDING STATE ********************************
//* ************************************************************************
// The FEEDING state handles the feed motor forward movement
// Motors remain enabled from the cutting state (no additional delay needed)

void enterFeedingState() {
  // Motors are already enabled from cutting state
  // But we still call this to ensure they're enabled if coming from elsewhere
  enableAllMotorsWithDelay();
  
  // Movement will start after delay is complete in updateFeedingState()
}

void updateFeedingState() {
  // Reset activity timer to keep motors enabled
  resetMotorTimeout();
  
  // Wait for motor enable delay before starting movement
  if (waitingForMotorEnable) {
    if (isMotorEnableDelayComplete()) {
      // Start the feed movement after delay
      if (feedMotor) {
        Serial.println("Starting feed motor forward movement (" + String(feedMotorSteps) + " steps)");
        feedMotor->move(feedMotorSteps);
      }
    }
    return;
  }
  
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