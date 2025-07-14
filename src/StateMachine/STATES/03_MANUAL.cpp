#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ MANUAL STATE *********************************
//* ************************************************************************
// The MANUAL state handles serial command operations
// Motors are enabled immediately (no delay) for responsive manual control

void enterManualState() {
  // Enable motors immediately for manual commands (no delay)
  enableAllMotors();
  
  // Set manual mode flag
  manualMode = true;
  
  Serial.println("Manual mode active - motors enabled");
}

void updateManualState() {
  // Reset activity timer to keep motors enabled during manual operations
  resetMotorTimeout();
  
  // Manual state is primarily driven by serial commands
  // The actual motor movements are handled in the serial command processor
}

void exitManualState() {
  // Reset manual mode flag
  manualMode = false;
  
  // Motors will be handled by the target state
  // If going to IDLE, motors will timeout after 2 seconds
} 