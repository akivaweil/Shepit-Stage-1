#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"
#include <Bounce2.h>

//* ************************************************************************
//* ************************ CUTTING STATE *********************************
//* ************************************************************************
// The CUTTING state handles the actual cutting operation in 3 focused steps:
// Step 1: Cut Wood - Moves cut motor forward by cutMotorSteps distance
// Step 2: Return Cut Motor - Returns cut motor to starting position using faster return speed
// Step 3: Check Conditions - Determines if cutting cycle should restart or transition to RELOAD
//
// This state is focused solely on cutting operations - no positioning or clamp management
// Each step waits for motor movement completion before proceeding to the next
// Automatic cycle restart if conditions are met (wood present only - run cycle switch ignored)
// Note: Run cycle switch state is ignored during cutting to prevent interruption of critical operations

// Cutting step enumeration for the 3-step process (unique to this state)
enum CuttingStateStep {
  CUTTING_STEP_CUT_WOOD,           // Step 1: Cut wood by moving cut motor forward
  CUTTING_STEP_RETURN_CUT_MOTOR,   // Step 2: Return cut motor to starting position
  CUTTING_STEP_CHECK_CONDITIONS    // Step 3: Check conditions for next cycle or transition
};

// Current cutting step tracking
static CuttingStateStep currentCuttingStep = CUTTING_STEP_CUT_WOOD;

// Motor movement tracking variables
static bool cutMotorStarted = false;
static bool returnMotorStarted = false;
static unsigned long cutMotorStartTime = 0;
static unsigned long returnMotorStartTime = 0;

// Motor position tracking for return movement
static float cutMotorStartPosition = 0.0;

// Wood presence monitoring is now handled centrally in Sensor_Setup.cpp

void enterCuttingState() {
  // Sensors are now initialized centrally in initializeStateMachine()
  

  
  // Ensure motors are enabled for cutting operation
  enableAllMotors();
  
  // Reset cutting step to start with Step 1
  currentCuttingStep = CUTTING_STEP_CUT_WOOD;
  
  // Reset motor movement tracking variables
  cutMotorStarted = false;
  returnMotorStarted = false;
  cutMotorStartTime = 0;
  returnMotorStartTime = 0;
  
  // Store starting position for return movement
  if (cutMotor) {
    cutMotorStartPosition = cutMotor->getCurrentPosition();
  }
  
  Serial.println("CUTTING: Entering cutting state - starting Step 1: Cut Wood");
}

void updateCuttingState() {
  // Sensors are now updated centrally in updateStateMachine()
  
  // Note: Run cycle switch state is ignored during cutting operation
  // Cutting operation will complete regardless of switch state
  // This prevents interruption of critical cutting operations
  
  // Execute current cutting step
  switch (currentCuttingStep) {
    case CUTTING_STEP_CUT_WOOD:
      updateCutWoodStep();
      break;
      
    case CUTTING_STEP_RETURN_CUT_MOTOR:
      updateReturnCutMotorStep();
      break;
      
    case CUTTING_STEP_CHECK_CONDITIONS:
      updateCheckConditionsStep();
      break;
  }
}

void exitCuttingState() {
  // Stop any ongoing motor movement
  if (cutMotor && cutMotor->isRunning()) {
    cutMotor->forceStop();
  }
  
  Serial.println("CUTTING: Exiting cutting state");
}

//* ************************************************************************
//* STEP 1: CUT WOOD - MOVE CUT MOTOR FORWARD *****************************
//* ************************************************************************
void updateCutWoodStep() {
  // Start cut motor movement if not already started
  if (!cutMotorStarted && cutMotor) {
    // Configure cut motor for cutting operation
    cutMotor->setSpeedInHz(cutMotorSpeed);
    cutMotor->setAcceleration(cutMotorAcceleration);
    
    // Move cut motor forward by cutMotorSteps
    cutMotor->move(cutMotorSteps);
    cutMotorStarted = true;
    cutMotorStartTime = millis();
    
    Serial.println("CUTTING: Step 1 - Cut motor moving forward by " + String(cutMotorSteps) + " steps");
  }
  
  // Wait for cutting movement to complete
  if (cutMotorStarted && cutMotor && !cutMotor->isRunning()) {
    Serial.println("CUTTING: Step 1 complete - Cut motor reached target position");
    
    // Move to next step
    currentCuttingStep = CUTTING_STEP_RETURN_CUT_MOTOR;
    cutMotorStarted = false;
    
    Serial.println("CUTTING: Moving to Step 2: Return Cut Motor");
  }
  
  // Safety timeout check for cutting movement
  if (cutMotorStarted && cutMotor && cutMotor->isRunning() && 
      (millis() - cutMotorStartTime >= 10000)) { // 10 second timeout
    Serial.println("CUTTING: ERROR - Cut motor timeout during cutting movement");
    if (cutMotor) {
      cutMotor->forceStop();
    }
    transitionToState(STATE_IDLE);
  }
}

//* ************************************************************************
//* STEP 2: RETURN CUT MOTOR - RETURN TO STARTING POSITION ****************
//* ************************************************************************
void updateReturnCutMotorStep() {
  // Start return movement if not already started
  if (!returnMotorStarted && cutMotor) {
    // Configure cut motor for faster return movement
    cutMotor->setSpeedInHz(cutMotorReturnSpeed);
    cutMotor->setAcceleration(cutMotorReturnAcceleration);
    
    // Calculate target position (return to starting position)
    float targetPosition = cutMotorStartPosition;
    cutMotor->moveTo(targetPosition);
    returnMotorStarted = true;
    returnMotorStartTime = millis();
    
    Serial.println("CUTTING: Step 2 - Cut motor returning to position " + String(targetPosition) + " at return speed");
  }
  
  // Wait for return movement to complete
  if (returnMotorStarted && cutMotor && !cutMotor->isRunning()) {
    Serial.println("CUTTING: Step 2 complete - Cut motor returned to starting position");
    
    // Move to next step
    currentCuttingStep = CUTTING_STEP_CHECK_CONDITIONS;
    returnMotorStarted = false;
    
    Serial.println("CUTTING: Moving to Step 3: Check Conditions");
  }
  
  // Safety timeout check for return movement
  if (returnMotorStarted && cutMotor && cutMotor->isRunning() && 
      (millis() - returnMotorStartTime >= 5000)) { // 5 second timeout for return
    Serial.println("CUTTING: ERROR - Cut motor timeout during return movement");
    if (cutMotor) {
      cutMotor->forceStop();
    }
    transitionToState(STATE_IDLE);
  }
}

//* ************************************************************************
//* STEP 3: CHECK CONDITIONS - DETERMINE NEXT ACTION **********************
//* ************************************************************************
void updateCheckConditionsStep() {
  // Check if conditions are met for automatic cycle restart
  // Note: Only wood presence matters - run cycle switch state is ignored during cutting
  bool woodStillPresent = isWoodPresent();
  
  if (woodStillPresent) {
    // Conditions met: automatically restart cutting cycle
    Serial.println("CUTTING: Step 3 - Conditions met - restarting cutting cycle");
    Serial.println("CUTTING: Wood present: " + String(woodStillPresent ? "YES" : "NO"));
    Serial.println("CUTTING: Run cycle switch state ignored during cutting operation");
    
    // Reset to Step 1 for next cutting cycle
    currentCuttingStep = CUTTING_STEP_CUT_WOOD;
    cutMotorStarted = false;
    returnMotorStarted = false;
    
    // Update starting position for next cycle
    if (cutMotor) {
      cutMotorStartPosition = cutMotor->getCurrentPosition();
    }
    
    Serial.println("CUTTING: Restarting cutting cycle - returning to Step 1");
  } else {
    // Conditions not met: transition to RELOAD state
    Serial.println("CUTTING: Step 3 - Conditions not met - transitioning to RELOAD state");
    Serial.println("CUTTING: Wood present: " + String(woodStillPresent ? "YES" : "NO"));
    Serial.println("CUTTING: No wood present - cutting cycle complete");
    
    transitionToState(STATE_RELOAD);
  }
} 