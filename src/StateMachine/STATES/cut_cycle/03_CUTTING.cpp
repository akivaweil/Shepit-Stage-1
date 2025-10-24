#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ CUTTING STATE *********************************
//* ************************************************************************

// Cutting step enumeration
enum CuttingStateStep {
  CUTTING_STEP_CUT_WOOD,
  CUTTING_STEP_RETURN_CUT_MOTOR,
  CUTTING_STEP_CHECK_CONDITIONS
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

void enterCuttingState() {
  enableAllMotors();
  
  currentCuttingStep = CUTTING_STEP_CUT_WOOD;
  
  cutMotorStarted = false;
  returnMotorStarted = false;
  cutMotorStartTime = 0;
  returnMotorStartTime = 0;
  
  if (cutMotor) {
    cutMotorStartPosition = cutMotor->getCurrentPosition();
  }
}

void updateCuttingState() {
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
  if (cutMotor && cutMotor->isRunning()) {
    cutMotor->forceStop();
  }
}

// Step 1: Cut Wood
void updateCutWoodStep() {
  if (!cutMotorStarted && cutMotor) {
    cutMotor->setSpeedInHz(cutMotorSpeed);
    cutMotor->setAcceleration(cutMotorAcceleration);
    cutMotor->move(cutMotorSteps);
    cutMotorStarted = true;
    cutMotorStartTime = millis();
  }
  
  if (cutMotorStarted && cutMotor && !cutMotor->isRunning()) {
    currentCuttingStep = CUTTING_STEP_RETURN_CUT_MOTOR;
    cutMotorStarted = false;
  }
  
  // Safety timeout check
  if (cutMotorStarted && cutMotor && cutMotor->isRunning() && 
      (millis() - cutMotorStartTime >= 10000)) {
    if (cutMotor) {
      cutMotor->forceStop();
    }
    transitionToState(STATE_IDLE);
  }
}

// Step 2: Return Cut Motor
void updateReturnCutMotorStep() {
  if (!returnMotorStarted && cutMotor) {
    cutMotor->setSpeedInHz(cutMotorReturnSpeed);
    cutMotor->setAcceleration(cutMotorReturnAcceleration);
    
    float targetPosition = cutMotorStartPosition;
    cutMotor->moveTo(targetPosition);
    returnMotorStarted = true;
    returnMotorStartTime = millis();
  }
  
  if (returnMotorStarted && cutMotor && !cutMotor->isRunning()) {
    currentCuttingStep = CUTTING_STEP_CHECK_CONDITIONS;
    returnMotorStarted = false;
  }
  
  // Safety timeout check
  if (returnMotorStarted && cutMotor && cutMotor->isRunning() && 
      (millis() - returnMotorStartTime >= 5000)) {
    if (cutMotor) {
      cutMotor->forceStop();
    }
    transitionToState(STATE_IDLE);
  }
}

// Step 3: Check Conditions
void updateCheckConditionsStep() {
  bool woodStillPresent = isWoodPresent();
  bool runCycleStillActive = isRunCycleSwitchActive();
  
  if (woodStillPresent && runCycleStillActive) {
    transitionToState(STATE_FEED_TO_DISTANCE);
  } else if (woodStillPresent && !runCycleStillActive) {
    transitionToState(STATE_IDLE);
  } else {
    transitionToState(STATE_RELOAD);
  }
}
