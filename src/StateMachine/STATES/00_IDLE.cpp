#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ IDLE STATE ***********************************
//* ************************************************************************
// The IDLE state is where the system waits for button press or manual commands
// Motors will disable after 3 seconds of inactivity in this state

void enterIdleState() {
  // Reset manual mode flag
  manualMode = false;
  
  // Check if this was an emergency stop before resetting the flag
  bool wasEmergencyStop = emergencyStopRequested;
  
  // Reset emergency stop flag
  emergencyStopRequested = false;
  
  if (wasEmergencyStop) {
    Serial.println("Emergency stop complete - system ready");
  } else {
    Serial.println("System ready - waiting for button press or manual command");
  }
  Serial.println("Motors enabled - will disable after 3 seconds of inactivity");
}

void updateIdleState() {
  // Motor timeout checking is handled in main loop via checkMotorTimeout()
}

void exitIdleState() {
  // Nothing specific needed when exiting idle state
  // Motor enable/disable is handled by the target state
} 