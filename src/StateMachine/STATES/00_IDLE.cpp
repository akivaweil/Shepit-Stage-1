#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ IDLE STATE ***********************************
//* ************************************************************************
// The IDLE state is where the system waits for button press or manual commands
// Motors will disable after 2 seconds of inactivity in this state

void enterIdleState() {
  // Reset activity timer on state entry
  resetMotorTimeout();
  
  // Reset manual mode flag
  manualMode = false;
  
  // Reset cutting phase to ensure clean start for next cycle
  currentCuttingPhase = CUT_FORWARD_PHASE;
  
  Serial.println("System ready - waiting for button press or manual command");
  Serial.println("Motors will disable after 2 seconds of inactivity");
}

void updateIdleState() {
  // Check for motor timeout (2 seconds of inactivity)
  checkMotorTimeout();
}

void exitIdleState() {
  // Nothing specific needed when exiting idle state
  // Motor enable/disable is handled by the target state
} 