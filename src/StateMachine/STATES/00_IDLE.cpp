#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"
#include <Bounce2.h>

//* ************************************************************************
//* ************************ IDLE STATE ***********************************
//* ************************************************************************
// The IDLE state is where the system waits for button press or manual commands
// Motors will automatically disable after 3 seconds of inactivity (sleep mode)
// Also constantly monitors for wood detection to automatically start reloading

// Wood sensor debouncer for IDLE state monitoring
static Bounce2::Button idleWoodSensor = Bounce2::Button();

void enterIdleState() {
  // Initialize wood sensor monitoring
  idleWoodSensor.attach(IS_WOOD_PIN, INPUT_PULLUP);
  idleWoodSensor.interval(50); // 50ms debounce
  
  // Enable motors when entering idle state
  enableAllMotors();
  
  // Reset manual mode flag
  manualMode = false;
  
  // Check if this was an emergency stop before resetting the flag
  bool wasEmergencyStop = emergencyStopRequested;
  
  // Reset emergency stop flag
  emergencyStopRequested = false;
  
  if (wasEmergencyStop) {
    Serial.println("Emergency stop complete - system ready");
  } else {
    Serial.println("System ready");
  }
}

void updateIdleState() {
  // Update wood sensor
  idleWoodSensor.update();
  
  // Check for wood detection (active LOW - sensor reads 0 when wood detected)
  if (idleWoodSensor.fell()) {
    Serial.println("WOOD DETECTED - Starting RELOADING sequence");
    transitionToState(STATE_RELOADING);
    return;
  }
  
  // Sleep mode functionality is handled by checkMotorTimeout() in main loop
}

void exitIdleState() {
  // Nothing specific needed when exiting idle state
  // Motor enable/disable is handled by the target state
} 