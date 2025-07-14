#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ FEEDING STATE ********************************
//* ************************************************************************
// The FEEDING state handles the feed motor forward movement followed by pullback
// Motors remain enabled from the cutting state (no additional delay needed)

// Feeding phase tracking
enum FeedingPhase {
  FEED_FORWARD_PHASE,
  FEED_PULLBACK_PHASE
};

static FeedingPhase currentFeedingPhase = FEED_FORWARD_PHASE;

void enterFeedingState() {
  // Motors are permanently enabled - start with forward movement phase
  currentFeedingPhase = FEED_FORWARD_PHASE;
  
  // Retract clamp before feed motor movement
  retractClamp();
  
  if (feedMotor) {
    Serial.println("Starting feed motor forward movement (" + String(feedMotorSteps) + " steps)");
    feedMotor->move(feedMotorSteps);
  }
}

void updateFeedingState() {
  // Motors are permanently enabled - no timeout management needed
  
  // Check if current phase movement is complete
  if (feedMotor && !feedMotor->isRunning()) {
    
    if (currentFeedingPhase == FEED_FORWARD_PHASE) {
      //! ************************************************************************
      //! PHASE 1 COMPLETE: FORWARD MOVEMENT DONE, START PULLBACK
      //! ************************************************************************
      Serial.println("Feed motor forward movement COMPLETE (" + String(feedMotorSteps) + " steps)");
      
      // Transition to pullback phase
      currentFeedingPhase = FEED_PULLBACK_PHASE;
      
      // Start pullback movement (negative direction)
      Serial.println("Starting feed motor pullback (" + String(feedMotorPullbackSteps) + " steps)");
      feedMotor->move(-feedMotorPullbackSteps);
      
    } else if (currentFeedingPhase == FEED_PULLBACK_PHASE) {
      //! ************************************************************************
      //! PHASE 2 COMPLETE: PULLBACK DONE, FEEDING SEQUENCE COMPLETE
      //! ************************************************************************
      Serial.println("Feed motor pullback COMPLETE (" + String(feedMotorPullbackSteps) + " steps)");
      
      // Extend clamp now that feed motor movement is complete
      extendClamp();
      
      // Feeding sequence complete, return to idle
      Serial.println("*** CUTTING CYCLE COMPLETE ***");
      transitionToState(STATE_IDLE);
    }
  }
}

void exitFeedingState() {
  // Reset feeding phase for next cycle
  currentFeedingPhase = FEED_FORWARD_PHASE;
  
  // Motors will be handled by the target state
  // If going to IDLE, motors will timeout after 2 seconds
} 