#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ IDLE STATE ***********************************
//* ************************************************************************
// The IDLE state is where the system waits for button press or manual commands
// Motors will disable after 2 seconds of inactivity in this state

void enterIdleState() {
  // Reset manual mode flag
  manualMode = false;
  
  Serial.println("System ready - waiting for button press or manual command");
  Serial.println("Motors are permanently enabled");
}

void updateIdleState() {
  // Motors are permanently enabled - no timeout checking needed
}

void exitIdleState() {
  // Nothing specific needed when exiting idle state
  // Motor enable/disable is handled by the target state
} 