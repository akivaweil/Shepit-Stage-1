#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ MANUAL STATE *********************************
//* ************************************************************************
// The MANUAL state is purely a serial command utility
// Motors are enabled immediately (no delay) for responsive manual control
// This state does not interfere with the main state machine logic
// It only handles serial commands and motor movements

void enterManualState() {
  // Reset feed motor timeout lock when entering this state
  // This ensures the lock is cleared regardless of which state we came from
  if (isFeedMotorTimeoutLocked()) {
    Serial.println("MANUAL: Resetting feed motor timeout lock from previous state");
    resetFeedMotorTimeoutLock();
  }
  
  // CRITICAL FIX: Reset feed motor control variables to ensure clean start
  // This prevents issues with feed motor control from previous states
  resetFeedMotorControlVariables();
  
  // Motors are permanently enabled - no need to enable them
  
  // Set manual mode flag for serial command processing only
  manualMode = true;
  
  Serial.println("Manual mode active - motors permanently enabled for serial commands");
  Serial.println("Type 'exit' to return to normal operation");
}

void updateManualState() {
  // Manual state is purely driven by serial commands
  // No state machine logic or motor monitoring here
  // The actual motor movements are handled in the serial command processor
  // This state just waits for serial input
}

void exitManualState() {
  // Reset manual mode flag
  manualMode = false;
  
  Serial.println("Exiting manual mode - returning to normal operation");
  
  // Motors will be handled by the target state
  // If going to IDLE, motors will timeout after 2 seconds
} 