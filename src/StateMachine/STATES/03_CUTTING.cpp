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
static Bounce2::Button distanceSensor = Bounce2::Button();

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
static bool feedMotorStarted = false;
static bool cutMotorStarted = false;
static bool distanceSensorTriggered = false;
static bool cutMotorForwardComplete = false;
static bool cutMotorReturnComplete = false;

void enterCuttingState() {
  // Check only wood presence at the beginning
  if (!isWoodPresent()) {
    Serial.println("CUTTING: NO WOOD DETECTED - Canceling cutting cycle");
    transitionToState(STATE_IDLE);
    return;
  }
  
  // Wood present - proceed with cutting cycle
  Serial.println("CUTTING: Wood detected - starting simple cutting cycle");
  Serial.println("CUTTING: NOTE: Cycle will complete regardless of cycle switch state during operation");
  
  // Initialize distance sensor
  distanceSensor.attach(WOOD_DISTANCE_SENSOR_PIN, INPUT);
  distanceSensor.interval(50); // 50ms debounce
  
  // Enable motors
  enableAllMotors();
  
  // Reset all step variables
  currentStep = STEP_ACTIVATE_MOTORS;
  feedMotorStarted = false;
  cutMotorStarted = false;
  distanceSensorTriggered = false;
  cutMotorForwardComplete = false;
  cutMotorReturnComplete = false;
  
  // Ensure feed motor is stopped before starting
  if (feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
    Serial.println("CUTTING: Stopped feed motor before starting cutting cycle");
  }
}

void updateCuttingState() {
  // Update distance sensor
  distanceSensor.update();
  
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
    Serial.println("CUTTING: Feed motor configured and ready");
  }
  
  // Activate cut motor
  if (cutMotor) {
    cutMotor->setSpeedInHz(cutMotorSpeed);
    cutMotor->setAcceleration(cutMotorAcceleration);
    Serial.println("CUTTING: Cut motor configured and ready");
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
  if (!feedMotorStarted && feedMotor) {
    Serial.println("CUTTING: Step 3 - Starting feed motor forward movement");
    feedMotor->runForward(); // Continuous forward movement
    feedMotorStarted = true;
  }
  
  // Check if distance sensor is triggered (active HIGH - HIGH when wood detected)
  if (distanceSensor.read() == HIGH && !distanceSensorTriggered) {
    Serial.println("CUTTING: Distance sensor triggered - stopping feed motor");
    distanceSensorTriggered = true;
    
    // Stop the feed motor
    if (feedMotor) {
      feedMotor->forceStop();
      feedMotorStarted = false;
      Serial.println("CUTTING: Feed motor stopped successfully");
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
    cutMotor->move(cutMotorSteps);
    cutMotorStarted = true;
  }
  
  // Check if cutting movement is complete
  if (cutMotorStarted && cutMotor && !cutMotor->isRunning()) {
    Serial.println("CUTTING: Cut motor forward movement complete");
    cutMotorForwardComplete = true;
    
    // Move to next step
    currentStep = STEP_RETURN_CUT_MOTOR;
    Serial.println("CUTTING: Step 5 complete - moving to Step 6 (Return Cut Motor)");
  }
}

//* ************************************************************************
//* *********************** STEP 6: RETURN CUT MOTOR **********************
//* ************************************************************************
void updateReturnCutMotorStep() {
  // Start cut motor return movement if not already started
  if (!cutMotorReturnComplete && cutMotor) {
    Serial.println("CUTTING: Step 6 - Starting cut motor return movement (" + String(cutMotorSteps) + " steps)");
    cutMotor->move(-cutMotorSteps);
  }
  
  // Check if return movement is complete
  if (cutMotor && !cutMotor->isRunning()) {
    Serial.println("CUTTING: Cut motor return movement complete");
    cutMotorReturnComplete = true;
    
    // Move to next step
    currentStep = STEP_CHECK_CONDITIONS;
    Serial.println("CUTTING: Step 6 complete - moving to Step 7 (Check Conditions)");
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
    
    // Reset step variables for next cycle
    currentStep = STEP_ACTIVATE_MOTORS;
    feedMotorStarted = false;
    cutMotorStarted = false;
    distanceSensorTriggered = false;
    cutMotorForwardComplete = false;
    cutMotorReturnComplete = false;
    
    Serial.println("CUTTING: Reset complete - starting new cycle at Step 1");
  } else {
    if (!isWoodPresent()) {
      Serial.println("CUTTING: No wood detected - cutting cycle complete, returning to IDLE");
    } else {
      Serial.println("CUTTING: Run cycle switch not active - cutting cycle complete, returning to IDLE");
    }
    
    // Return to idle state
    transitionToState(STATE_IDLE);
  }
}

void exitCuttingState() {
  // Ensure feed motor is stopped before exiting
  if (feedMotor && feedMotor->isRunning()) {
    Serial.println("CUTTING: Exit - stopping feed motor");
    feedMotor->forceStop();
  }
  
  // Reset all step variables
  currentStep = STEP_ACTIVATE_MOTORS;
  feedMotorStarted = false;
  cutMotorStarted = false;
  distanceSensorTriggered = false;
  cutMotorForwardComplete = false;
  cutMotorReturnComplete = false;
  
  Serial.println("CUTTING: Exit - all variables reset");
} 