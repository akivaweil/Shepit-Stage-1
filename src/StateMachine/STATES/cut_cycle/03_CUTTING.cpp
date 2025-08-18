#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"
#include <Bounce2.h>

//* ************************************************************************
//* ************************ CUTTING STATE ********************************
//* ************************************************************************
// The CUTTING state implements a simple 7-step cutting cycle:
// 1. Activate the cut and feed motors
// 2. Retract the clamp
// 3. Feed wood forward until sensor triggers
// 4. Extend the clamp
// 5. Cut wood by moving the cut motor forward
// 6. Return cut motor
// 7. Check whether the run cycle && wood present sensors are active. If they are, return to step 1. If not, return to idle state and wait.

// Wood distance sensor debouncer
static Bounce2::Button cuttingDistanceSensor = Bounce2::Button();

// Cutting cycle steps
enum CuttingStep {
  STEP_ACTIVATE_MOTORS,    // Step 1: Activate cut and feed motors
  STEP_RETRACT_CLAMP,      // Step 2: Retract the clamp
  STEP_FEED_FORWARD,       // Step 3: Feed wood forward until sensor triggers
  STEP_EXTEND_CLAMP,       // Step 4: Extend the clamp
  STEP_CUT_WOOD,           // Step 5: Cut wood by moving cut motor forward
  STEP_RETURN_CUT_MOTOR,   // Step 6: Return cut motor
  STEP_CHECK_CONDITIONS    // Step 7: Check conditions for next cycle
};

static CuttingStep currentStep = STEP_ACTIVATE_MOTORS;

//! ************************************************************************
//! CUT CYCLE STATE VARIABLES - RESET AT BEGINNING OF EACH CYCLE
//! ************************************************************************
// These variables track the state of each step and must be reset for each new cycle

// Cut motor step tracking variables
static bool cutMotorStarted = false;
static int32_t cutStartPosition = 0;

// Return motor step tracking variables  
static bool returnMotorStarted = false;
static int32_t returnStartPosition = 0;

void enterCuttingState() {
  // Reset feed motor timeout lock when entering this state
  // This ensures the lock is cleared regardless of which state we came from
  if (isFeedMotorTimeoutLocked()) {
    Serial.println("CUTTING: Resetting feed motor timeout lock from previous state");
    resetFeedMotorTimeoutLock();
  }
  
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
  
  // Enable motors
  enableAllMotors();
  
  // Start at step 1
  currentStep = STEP_ACTIVATE_MOTORS;
  
  // Ensure feed motor is stopped before starting
  if (feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
  }
  
  //! ************************************************************************
  //! RESET ALL CUT CYCLE FLAGS AND VARIABLES
  //! ************************************************************************
  // Reset all static variables and flags to ensure clean start of each cycle
  
  // Reset cut motor step flags
  resetCutMotorStepFlags();
  
  // Reset return motor step flags  
  resetReturnMotorStepFlags();
  
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
  if (!cutMotorStarted && cutMotor) {
    Serial.println("CUTTING: Step 5 - Starting cut motor forward movement (" + String(cutMotorSteps) + " steps)");
    
    // Store starting position
    cutStartPosition = cutMotor->getCurrentPosition();
    Serial.println("CUTTING: Cut motor starting position: " + String(cutStartPosition));
    
    // Configure motor for forward movement
    cutMotor->setSpeedInHz(cutMotorSpeed);
    cutMotor->setAcceleration(cutMotorAcceleration);
    
    // Start forward movement
    cutMotor->move(cutMotorSteps);
    cutMotorStarted = true;
  }
  
  // Check if cutting movement is complete
  if (cutMotorStarted && cutMotor && !cutMotor->isRunning()) {
    int32_t currentPosition = cutMotor->getCurrentPosition();
    int32_t expectedPosition = cutStartPosition + cutMotorSteps;
    
    Serial.println("CUTTING: Cut motor forward movement complete");
    Serial.println("CUTTING: Final position: " + String(currentPosition) + ", Expected: " + String(expectedPosition));
    
    // Verify position is close to expected (allow small tolerance)
    if (abs(currentPosition - expectedPosition) <= 10) {
      Serial.println("CUTTING: Position verification successful - moving to return step");
      
      // Reset for return movement
      cutMotorStarted = false;
      
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
  if (!returnMotorStarted && cutMotor) {
    Serial.println("CUTTING: Step 6 - Starting cut motor return movement (" + String(cutMotorSteps) + " steps)");
    
    // Store starting position for return
    returnStartPosition = cutMotor->getCurrentPosition();
    Serial.println("CUTTING: Return starting position: " + String(returnStartPosition));
    
    // Configure motor for return movement (faster speed)
    cutMotor->setSpeedInHz(cutMotorReturnSpeed);
    cutMotor->setAcceleration(cutMotorReturnAcceleration);
    
    // Start return movement
    cutMotor->move(-cutMotorSteps);
    returnMotorStarted = true;
  }
  
  // Check if return movement is complete
  if (returnMotorStarted && cutMotor && !cutMotor->isRunning()) {
    int32_t currentPosition = cutMotor->getCurrentPosition();
    int32_t expectedPosition = returnStartPosition - cutMotorSteps;
    
    Serial.println("CUTTING: Cut motor return movement complete");
    Serial.println("CUTTING: Final return position: " + String(currentPosition) + ", Expected: " + String(expectedPosition));
    
    // Verify return position is close to expected (allow small tolerance)
    if (abs(currentPosition - expectedPosition) <= 10) {
      Serial.println("CUTTING: Return position verification successful");
      
      // Reset for next cycle
      returnMotorStarted = false;
      
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
    Serial.println("CUTTING: Automatically transitioning to RELOAD state to clear cutting area");
    transitionToState(STATE_RELOAD);
  }
}

//* ************************************************************************
//* *********************** FLAG RESET FUNCTIONS **************************
//* ************************************************************************
// These functions reset all static variables and flags used in the cutting cycle steps
// They ensure each new cutting cycle starts with clean state

void resetCutMotorStepFlags() {
  // Reset cut motor step tracking variables
  cutMotorStarted = false;
  cutStartPosition = 0;
  
  Serial.println("CUTTING: Cut motor step flags reset");
}

void resetReturnMotorStepFlags() {
  // Reset return motor step tracking variables
  returnMotorStarted = false;
  returnStartPosition = 0;
  
  Serial.println("CUTTING: Return motor step flags reset");
}

void exitCuttingState() {
  // Ensure feed motor is stopped before exiting
  if (feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
  }
  
  // Reset to step 1
  currentStep = STEP_ACTIVATE_MOTORS;
} 