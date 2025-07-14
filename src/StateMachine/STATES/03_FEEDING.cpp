#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ FEEDING STATE ********************************
//* ************************************************************************
// The FEEDING state handles the feed motor forward movement, 5-second delay, 
// and return movement back to original position
// Motors remain enabled from the cutting state (no additional delay needed)

void enterFeedingState() {
  // Initialize feeding phase to forward movement
  currentFeedingPhase = FEED_FORWARD_PHASE;
  
  // Motors are permanently enabled - start feed movement immediately
  if (feedMotor) {
    Serial.println("Starting feed motor forward movement (" + String(feedMotorSteps) + " steps)");
    feedMotor->move(feedMotorSteps);
  }
}

void updateFeedingState() {
  // Motors are permanently enabled - no timeout management needed
  
  switch (currentFeedingPhase) {
    case FEED_FORWARD_PHASE:
      //! ************************************************************************
      //! PHASE 1: MOVE FEED MOTOR FORWARD
      //! ************************************************************************
      // Check if feed motor forward movement is complete
      if (feedMotor && !feedMotor->isRunning()) {
        Serial.println("Feed motor forward movement COMPLETE - starting 5 second delay");
        
        // Start the 5-second delay timer
        feedReturnDelayStartTime = millis();
        currentFeedingPhase = FEED_WAITING_PHASE;
      }
      break;
      
    case FEED_WAITING_PHASE:
      //! ************************************************************************
      //! PHASE 2: WAIT 5 SECONDS BEFORE RETURN MOVEMENT
      //! ************************************************************************
      // Check if 5-second delay has elapsed
      if (millis() - feedReturnDelayStartTime >= feedReturnDelayMs) {
        Serial.println("5 second delay COMPLETE - starting return movement (" + String(feedMotorReturnSteps) + " steps)");
        
        // Start return movement
        if (feedMotor) {
          feedMotor->move(feedMotorReturnSteps);
        }
        currentFeedingPhase = FEED_RETURN_PHASE;
      }
      break;
      
    case FEED_RETURN_PHASE:
      //! ************************************************************************
      //! PHASE 3: MOVE FEED MOTOR BACK TO ORIGINAL POSITION
      //! ************************************************************************
      // Check if feed motor return movement is complete
      if (feedMotor && !feedMotor->isRunning()) {
        Serial.println("Feed motor return movement COMPLETE");
        
        // Feeding sequence complete, return to idle
        Serial.println("*** CUTTING CYCLE COMPLETE ***");
        transitionToState(STATE_IDLE);
      }
      break;
  }
}

void exitFeedingState() {
  // Reset feeding phase for next cycle
  currentFeedingPhase = FEED_FORWARD_PHASE;
  
  // Motors will be handled by the target state
  // If going to IDLE, motors will timeout after 2 seconds
} 