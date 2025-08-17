#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ MANUAL STATE *********************************
//* ************************************************************************
// The MANUAL state handles serial command operations
// Motors are enabled immediately (no delay) for responsive manual control

void enterManualState() {
  // Motors are permanently enabled - no need to enable them
  
  // Set manual mode flag
  manualMode = true;
  
  Serial.println("Manual mode active - motors permanently enabled");
}

void updateManualState() {
  // Motors are permanently enabled - no timeout management needed
  
  // Manual state is primarily driven by serial commands
  // The actual motor movements are handled in the serial command processor
}

void exitManualState() {
  // Reset manual mode flag
  manualMode = false;
  
  // Motors will be handled by the target state
  // If going to IDLE, motors will timeout after 2 seconds
} 