#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ RETURNING STATE ******************************
//* ************************************************************************
// The RETURNING state handles the cut motor backward movement with different settings
// Motors are permanently enabled - uses specific return speed and acceleration
// After return movement, handles feeding forward and pullback before next cutting cycle

// Feeding phase tracking
enum FeedingPhase {
  FEED_FORWARD_PHASE,
  FEED_PULLBACK_PHASE
};

static FeedingPhase currentFeedingPhase = FEED_FORWARD_PHASE;
static bool feedingStarted = false;

void enterReturningState() {
  // Check both conditions at the beginning of returning state
  if (!isRunCycleSwitchActive() || !isWoodPresent()) {
    if (!isRunCycleSwitchActive()) {
      Serial.println("RETURNING: RUN CYCLE SWITCH NOT ACTIVE - Canceling cutting cycle");
    } else if (!isWoodPresent()) {
      Serial.println("RETURNING: NO WOOD DETECTED - Canceling cutting cycle");
    }
    
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
  
  // Conditions met - proceed with returning state
  Serial.println("RETURNING: Conditions verified - RUN CYCLE SWITCH ACTIVE & WOOD DETECTED");
  
  // Motors are permanently enabled - configure motors for return movement
  
  // Retract clamp before feed motor movement
  retractClamp();
  
  //! ************************************************************************
  //! STEP 1: CONFIGURE AND START FEED MOTOR PULLBACK (SIMULTANEOUS)
  //! ************************************************************************
  if (feedMotor) {
    // Use existing feed motor settings for pullback movement
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
    
    // Start feed motor pullback movement (negative direction)
    Serial.println("Starting feed motor pullback (" + String(FM_returnPullback) + " steps)");
    feedMotor->move(-FM_returnPullback);
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
  
  // Reset feeding phase tracking
  currentFeedingPhase = FEED_FORWARD_PHASE;
  feedingStarted = false;
}

void updateReturningState() {
  // Motors are permanently enabled - no timeout management needed
  
  // Check if both motors have completed their movements
  bool feedMotorComplete = !feedMotor || !feedMotor->isRunning();
  bool cutMotorComplete = !cutMotor || !cutMotor->isRunning();
  
  if (feedMotorComplete && cutMotorComplete && !feedingStarted) {
    Serial.println("Feed motor pullback COMPLETE");
    Serial.println("Cut motor return movement COMPLETE");
    
    // Extend clamp now that feed motor movement is complete
    extendClamp();
    
    // Restore original cut motor settings for future cutting operations
    if (cutMotor) {
      cutMotor->setSpeedInHz(cutMotorSpeed);
      cutMotor->setAcceleration(cutMotorAcceleration);
    }
    
    // Start feeding sequence for next cutting cycle
    feedingStarted = true;
    currentFeedingPhase = FEED_FORWARD_PHASE;
    
    // Retract clamp before feed motor movement
    retractClamp();
    
    if (feedMotor) {
      Serial.println("Starting feed motor forward movement (" + String(feedMotorSteps) + " steps)");
      feedMotor->move(feedMotorSteps);
    }
  }
  
  // Handle feeding sequence after return movement is complete
  if (feedingStarted && feedMotor && !feedMotor->isRunning()) {
    
    if (currentFeedingPhase == FEED_FORWARD_PHASE) {
      //! ************************************************************************
      //! PHASE 1 COMPLETE: FORWARD MOVEMENT DONE, START PULLBACK
      //! ************************************************************************
      Serial.println("Feed motor forward movement COMPLETE (" + String(feedMotorSteps) + " steps)");
      
      // Transition to pullback phase
      currentFeedingPhase = FEED_PULLBACK_PHASE;
      
      // Start pullback movement (negative direction)
      Serial.println("Starting feed motor pullback (" + String(FM_preCutPullback) + " steps)");
      feedMotor->move(-FM_preCutPullback);
      
    } else if (currentFeedingPhase == FEED_PULLBACK_PHASE) {
      //! ************************************************************************
      //! PHASE 2 COMPLETE: PULLBACK DONE, FEEDING SEQUENCE COMPLETE
      //! ************************************************************************
      Serial.println("Feed motor pullback COMPLETE (" + String(FM_preCutPullback) + " steps)");
      
      // Extend clamp now that feed motor movement is complete
      extendClamp();
      
      //! ************************************************************************
      //! CHECK WOOD SENSOR FOR CONTINUOUS CUTTING
      //! ************************************************************************
      if (isWoodPresent() && isRunCycleSwitchActive()) {
        Serial.println("Wood still present & run cycle switch active - starting another cutting cycle");
        transitionToState(STATE_CUTTING);
      } else {
        // Check why we're not continuing
        if (!isWoodPresent()) {
          Serial.println("*** CUTTING CYCLE COMPLETE - No more wood detected ***");
        } else if (!isRunCycleSwitchActive()) {
          Serial.println("*** CUTTING CYCLE CANCELLED - Run cycle switch turned off ***");
        }
        transitionToState(STATE_IDLE);
      }
    }
  }
}

void exitReturningState() {
  // Reset feeding phase for next cycle
  currentFeedingPhase = FEED_FORWARD_PHASE;
  feedingStarted = false;
  
  // Motors will be handled by the target state
  // If going to IDLE, motors will timeout after 2 seconds
} 