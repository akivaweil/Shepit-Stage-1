#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"
#include <Bounce2.h>

//* ************************************************************************
//* ************************ CUTTING STATE ********************************
//* ************************************************************************
// The CUTTING state handles the complete cutting cycle including:
// 1. Activate motors (cut motor enabled, feed motor enabled)
// 2. Retract clamp for movement
// 3. Feed wood forward until distance sensor triggers
// 4. Extend clamp to secure wood
// 5. Cut wood by moving cut motor forward
// 6. Return cut motor to starting position
// 7. Check conditions for next cycle or complete
//
// This state automatically cycles through all steps and can restart
// for multiple cuts if conditions remain met (wood present + run cycle active)
// After completion, automatically transitions to RELOAD state to clear cutting area

// Distance sensor debouncer for cutting cycle
static Bounce2::Button cuttingDistanceSensor = Bounce2::Button();

// Cutting cycle step tracking
static CuttingStep currentStep = STEP_ACTIVATE_MOTORS;

// SIMPLIFIED: Only essential motor position tracking
static int32_t cutStartPosition = 0;
static int32_t returnStartPosition = 0;

void enterCuttingState() {
  // Reset feed motor timeout lock when entering this state
  // This ensures the lock is cleared regardless of which state we came from
  if (isFeedMotorTimeoutLocked()) {
    Serial.println("CUTTING: Resetting feed motor timeout lock from previous state");
    resetFeedMotorTimeoutLock();
  }
  
  // CRITICAL FIX: Reset feed motor control variables to ensure clean start
  // This prevents issues with feed motor control from previous states
  resetFeedMotorControlVariables();
  
  // Check only wood presence at the beginning
  if (!isWoodPresent()) {
    Serial.println("CUTTING: NO WOOD DETECTED - Canceling cutting cycle");
    transitionToState(STATE_IDLE);
    return;
  }
  
  // Wood present - proceed with cutting cycle
  Serial.println("CUTTING: Wood detected - starting simple cutting cycle");
  
  // Initialize distance sensor with INPUT mode (active HIGH - HIGH when wood detected)
  cuttingDistanceSensor.attach(WOOD_DISTANCE_SENSOR_PIN, INPUT);
  cuttingDistanceSensor.interval(distanceSensorDebounceTime); // Distance sensor debounce
  
  // Reset cutting cycle flags for new cycle
  resetCuttingCycleFlags();
  
  // Enable motors
  enableAllMotors();
  
  // Ensure feed motor is stopped before starting
  if (feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
  }
  
  Serial.println("CUTTING: All cut cycle flags and variables reset for new cycle");
}

void updateCuttingState() {
  // Update distance sensor
  cuttingDistanceSensor.update();
  
  // Handle different steps of the cutting cycle
  switch (currentStep) {
    case STEP_ACTIVATE_MOTORS:
      updateActivateMotorsStep();
      break;
      
    case STEP_RETRACT_CLAMP:
      updateRetractClampStep();
      break;
      
    case STEP_FEED_FORWARD:
      updateFeedForwardStep();
      break;
      
    case STEP_EXTEND_CLAMP:
      updateExtendClampStep();
      break;
      
    case STEP_CUT_WOOD:
      updateCutWoodStep();
      break;
      
    case STEP_RETURN_CUT_MOTOR:
      updateReturnCutMotorStep();
      break;
      
    case STEP_CHECK_CONDITIONS:
      updateCheckConditionsStep();
      break;
  }
}

//* ************************************************************************
//* *********************** STEP 1: ACTIVATE MOTORS ***********************
//* ************************************************************************
void updateActivateMotorsStep() {
  Serial.println("CUTTING: Step 1 - Activating cut and feed motors");
  
  // Activate feed motor
  if (feedMotor) {
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
  }
  
  // Activate cut motor
  if (cutMotor) {
    cutMotor->setSpeedInHz(cutMotorSpeed);
    cutMotor->setAcceleration(cutMotorAcceleration);
  }
  
  // Move to next step
  currentStep = STEP_RETRACT_CLAMP;
  Serial.println("CUTTING: Step 1 complete - moving to Step 2 (Retract Clamp)");
}

//* ************************************************************************
//* *********************** STEP 2: RETRACT CLAMP *************************
//* ************************************************************************
void updateRetractClampStep() {
  Serial.println("CUTTING: Step 2 - Retracting clamp");
  
  // Retract the clamp
  retractClamp();
  
  // Brief delay to ensure clamp movement completes
  delay(100);
  
  // Move to next step
  currentStep = STEP_FEED_FORWARD;
  Serial.println("CUTTING: Step 2 complete - moving to Step 3 (Feed Forward)");
}

//* ************************************************************************
//* *********************** STEP 3: FEED FORWARD **************************
//* ************************************************************************
void updateFeedForwardStep() {
  // Start feed motor if not already started
  if (feedMotor && !feedMotor->isRunning()) {
    Serial.println("CUTTING: Step 3 - Starting feed motor forward movement");
    feedMotor->runForward(); // Continuous forward movement
  }
  
  // Check if distance sensor is triggered (active HIGH - HIGH when wood detected)
  if (cuttingDistanceSensor.read() == HIGH) {
    Serial.println("CUTTING: Distance sensor triggered - stopping feed motor");
    
    // Stop the feed motor
    if (feedMotor) {
      feedMotor->forceStop();
    }
    
    // Move to next step
    currentStep = STEP_EXTEND_CLAMP;
    Serial.println("CUTTING: Step 3 complete - moving to Step 4 (Extend Clamp)");
  }
}

//* ************************************************************************
//* *********************** STEP 4: EXTEND CLAMP **************************
//* ************************************************************************
void updateExtendClampStep() {
  Serial.println("CUTTING: Step 4 - Extending clamp");
  
  // Extend the clamp to secure wood
  extendClamp();
  
  // Brief delay to ensure clamp movement completes
  delay(100);
  
  // Move to next step
  currentStep = STEP_CUT_WOOD;
  Serial.println("CUTTING: Step 4 complete - moving to Step 5 (Cut Wood)");
}

//* ************************************************************************
//* *********************** STEP 5: CUT WOOD ******************************
//* ************************************************************************
void updateCutWoodStep() {
  // Start cut motor forward movement if not already started
  if (cutMotor) {
    Serial.println("CUTTING: Step 5 - Starting cut motor forward movement (" + String(cutMotorSteps) + " steps)");
    
    // Store starting position
    cutStartPosition = cutMotor->getCurrentPosition();
    Serial.println("CUTTING: Cut motor starting position: " + String(cutStartPosition));
    
    // Configure motor for forward movement
    cutMotor->setSpeedInHz(cutMotorSpeed);
    cutMotor->setAcceleration(cutMotorAcceleration);
    
    // Start forward movement
    cutMotor->move(cutMotorSteps);
  }
  
  // Check if cutting movement is complete
  if (cutMotor && !cutMotor->isRunning()) {
    int32_t currentPosition = cutMotor->getCurrentPosition();
    int32_t expectedPosition = cutStartPosition + cutMotorSteps;
    
    Serial.println("CUTTING: Cut motor forward movement complete");
    Serial.println("CUTTING: Final position: " + String(currentPosition) + ", Expected: " + String(expectedPosition));
    
    // Verify position is close to expected (allow small tolerance)
    if (abs(currentPosition - expectedPosition) <= 10) {
      Serial.println("CUTTING: Position verification successful - moving to return step");
      
      // Move to next step
      currentStep = STEP_RETURN_CUT_MOTOR;
      Serial.println("CUTTING: Step 5 complete - moving to Step 6 (Return Cut Motor)");
    } else {
      Serial.println("CUTTING: WARNING - Position verification failed, motor may not have moved correctly");
      // Try to move to correct position
      cutMotor->moveTo(expectedPosition);
    }
  }
}

//* ************************************************************************
//* *********************** STEP 6: RETURN CUT MOTOR **********************
//* ************************************************************************
void updateReturnCutMotorStep() {
  // Start cut motor return movement if not already started
  if (cutMotor) {
    Serial.println("CUTTING: Step 6 - Starting cut motor return movement (" + String(cutMotorSteps) + " steps)");
    
    // Store starting position for return
    returnStartPosition = cutMotor->getCurrentPosition();
    Serial.println("CUTTING: Return starting position: " + String(returnStartPosition));
    
    // Configure motor for return movement (faster speed)
    cutMotor->setSpeedInHz(cutMotorReturnSpeed);
    cutMotor->setAcceleration(cutMotorReturnAcceleration);
    
    // Start return movement
    cutMotor->move(-cutMotorSteps);
  }
  
  // Check if return movement is complete
  if (cutMotor && !cutMotor->isRunning()) {
    int32_t currentPosition = cutMotor->getCurrentPosition();
    int32_t expectedPosition = returnStartPosition - cutMotorSteps;
    
    Serial.println("CUTTING: Cut motor return movement complete");
    Serial.println("CUTTING: Final return position: " + String(currentPosition) + ", Expected: " + String(expectedPosition));
    
    // Verify return position is close to expected (allow small tolerance)
    if (abs(currentPosition - expectedPosition) <= 10) {
      Serial.println("CUTTING: Return position verification successful");
      
      // Move to next step
      currentStep = STEP_CHECK_CONDITIONS;
      Serial.println("CUTTING: Step 6 complete - moving to Step 7 (Check Conditions)");
    } else {
      Serial.println("CUTTING: WARNING - Return position verification failed, motor may not have returned correctly");
      // Try to move to correct return position
      cutMotor->moveTo(expectedPosition);
    }
  }
}

//* ************************************************************************
//* *********************** STEP 7: CHECK CONDITIONS **********************
//* ************************************************************************
void updateCheckConditionsStep() {
  Serial.println("CUTTING: Step 7 - Checking conditions for next cycle");
  
  // Check both wood presence AND run cycle switch status
  if (isWoodPresent() && isRunCycleSwitchActive()) {
    Serial.println("CUTTING: Conditions met - starting another cutting cycle");
    
    // Reset to step 1 for next cycle
    currentStep = STEP_ACTIVATE_MOTORS;
    Serial.println("CUTTING: Reset complete - starting new cycle at Step 1");
  } else {
    if (!isWoodPresent()) {
      Serial.println("CUTTING: No wood detected - cutting cycle complete, transitioning to RELOAD");
    } else {
      Serial.println("CUTTING: Run cycle switch not active - cutting cycle complete, transitioning to RELOAD");
    }
    
    //! ************************************************************************
    //! AUTOMATIC TRANSITION TO RELOAD AFTER CUTTING
    //! ************************************************************************
    // After cutting is complete, automatically transition to reload to clear the cutting area
    // This ensures the operator doesn't have to reach in near the spinning saw blade
    
    // Reset all cutting cycle flags before transitioning
    resetCuttingCycleFlags();
    
    // CRITICAL FIX: Reset feed motor control variables to ensure clean state
    // This prevents issues with feed motor not responding to cycle switch after cutting
    resetFeedMotorControlVariables();
    
    Serial.println("CUTTING: Automatically transitioning to RELOAD state to clear cutting area");
    transitionToState(STATE_RELOAD);
  }
}

//* ************************************************************************
//* *********************** EXIT FUNCTION **************************
//* ************************************************************************

void exitCuttingState() {
  // Ensure feed motor is stopped before exiting
  if (feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
  }
  
  // Note: Cutting cycle flags are now reset by resetCuttingCycleFlags() 
  // which is called before transitioning to RELOAD state
} 