#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ CUTTING STATE ********************************
//* ************************************************************************
// The CUTTING state handles the cut motor forward movement
// Motors are enabled with delay to ensure proper wake-up from sleep mode

static bool cutMotorStarted = false;

void enterCuttingState() {
  // Check both conditions at the beginning of each cutting cycle
  if (!isRunCycleSwitchActive() || !isWoodPresent()) {
    if (!isRunCycleSwitchActive()) {
      Serial.println("CUTTING: RUN CYCLE SWITCH NOT ACTIVE - Canceling cutting cycle");
    } else if (!isWoodPresent()) {
      Serial.println("CUTTING: NO WOOD DETECTED - Canceling cutting cycle");
    }
    
    // Return to idle state
    transitionToState(STATE_IDLE);
    return;
  }
  
  // Conditions met - proceed with cutting cycle
  Serial.println("CUTTING: Conditions verified - RUN CYCLE SWITCH ACTIVE & WOOD DETECTED");
  
  // Enable motors with delay to ensure proper wake-up from sleep mode
  enableAllMotorsWithDelay();
  
  // Reset movement tracking flag
  cutMotorStarted = false;
}

void updateCuttingState() {
  // Check both conditions at the beginning of each update cycle
  if (!isRunCycleSwitchActive() || !isWoodPresent()) {
    if (!isRunCycleSwitchActive()) {
      Serial.println("CUTTING: RUN CYCLE SWITCH NOT ACTIVE - Canceling cutting cycle");
    } else if (!isWoodPresent()) {
      Serial.println("CUTTING: NO WOOD DETECTED - Canceling cutting cycle");
    }
    
    // Stop cut motor if it's running
    if (cutMotor && cutMotor->isRunning()) {
      cutMotor->forceStop();
    }
    
    // Return to idle state
    transitionToState(STATE_IDLE);
    return;
  }
  
  // Wait for motor enable delay to complete before starting movement
  if (!isMotorEnableDelayComplete()) {
    return; // Still waiting for motor stabilization
  }
  
  // Start cut motor movement if not already started
  if (!cutMotorStarted && cutMotor) {
    Serial.println("Starting cut motor forward movement (" + String(cutMotorSteps) + " steps)");
    cutMotor->move(cutMotorSteps);
    cutMotorStarted = true;
  }
  
  // Check if movement is complete
  if (cutMotorStarted && cutMotor && !cutMotor->isRunning()) {
    Serial.println("Cut motor forward movement COMPLETE");
    
    // Check conditions again before transitioning to returning
    if (isRunCycleSwitchActive() && isWoodPresent()) {
      // Conditions still met - continue with returning state
      transitionToState(STATE_RETURNING);
    } else {
      // Conditions no longer met - cancel cycle
      if (!isRunCycleSwitchActive()) {
        Serial.println("CUTTING: RUN CYCLE SWITCH NOT ACTIVE - Canceling cutting cycle");
      } else if (!isWoodPresent()) {
        Serial.println("CUTTING: NO WOOD DETECTED - Canceling cutting cycle");
      }
      transitionToState(STATE_IDLE);
    }
  }
}

void exitCuttingState() {
  // Reset movement tracking flag for next cycle
  cutMotorStarted = false;
  
  // Motors stay enabled for the next state
  // No need to disable motors here
} 