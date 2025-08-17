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
// Run cycle switch monitoring for cutting cycle control

// Wood sensor debouncer for IDLE state monitoring
static Bounce2::Button idleWoodSensor = Bounce2::Button();

// Run cycle switch debouncer for cutting cycle control
static Bounce2::Button runCycleSwitch = Bounce2::Button();

// Right switch debouncer for continuous feed operation
static Bounce2::Button rightSwitch = Bounce2::Button();

// Red button debouncer for continuous feed operation
static Bounce2::Button redButton = Bounce2::Button();

// Distance sensor debouncer for test mode
static Bounce2::Button distanceSensor = Bounce2::Button();

void enterIdleState() {
  // Initialize wood sensor monitoring
  idleWoodSensor.attach(WOOD_PRESENT_SENSOR_PIN, INPUT_PULLUP);
  idleWoodSensor.interval(50); // 50ms debounce
  
  // Initialize run cycle switch monitoring
  runCycleSwitch.attach(RUN_CYCLE_SWITCH_PIN, INPUT_PULLDOWN);
  runCycleSwitch.interval(50); // 50ms debounce
  
  // Initialize right switch monitoring
  rightSwitch.attach(RIGHT_SWITCH_PIN, INPUT_PULLDOWN);
  rightSwitch.interval(50); // 50ms debounce
  
  // Initialize red button monitoring
  redButton.attach(RED_BUTTON_PIN, INPUT_PULLDOWN);
  redButton.interval(50); // 50ms debounce
  
  // Initialize distance sensor monitoring for test mode
  distanceSensor.attach(WOOD_DISTANCE_SENSOR_PIN, INPUT_PULLDOWN);
  distanceSensor.interval(50); // 50ms debounce
  
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
  
  // TEST MODE: Start feed motor running continuously on startup
  Serial.println("*** TEST MODE: Starting feed motor continuously until distance sensor triggered ***");
  if (feedMotor) {
    // Retract clamp for feed motor movement
    retractClamp();
    
    // Start feed motor running forward continuously
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
    feedMotor->runForward();
    
    Serial.println("Feed motor started - running continuously until distance sensor on pin 12 is triggered");
  }
}

void updateIdleState() {
  // Update wood sensor
  idleWoodSensor.update();
  
  // Update run cycle switch
  runCycleSwitch.update();
  
  // Update right switch
  rightSwitch.update();
  
  // Update red button
  redButton.update();
  
  // Update distance sensor
  distanceSensor.update();
  
  // Simple distance sensor logic: run when NOT triggered, stop when triggered
  if (distanceSensor.read() == HIGH) {
    // Sensor triggered - stop motor if running
    if (feedMotor && feedMotor->isRunning()) {
      feedMotor->forceStop();
      extendClamp();
      Serial.println("Distance sensor triggered - motor stopped");
    }
  } else {
    // Sensor not triggered - start motor if not running
    if (feedMotor && !feedMotor->isRunning()) {
      retractClamp();
      // Reconfigure motor settings before starting
      feedMotor->setSpeedInHz(feedMotorSpeed);
      feedMotor->setAcceleration(feedMotorAcceleration);
      feedMotor->runForward();
      Serial.println("Distance sensor not triggered - motor started with speed: " + String(feedMotorSpeed));
    }
  }
  
  // Debug output every few seconds
  static unsigned long lastDebugTime = 0;
  if (millis() - lastDebugTime > 2000) { // Every 2 seconds
    lastDebugTime = millis();
    Serial.println("Debug - Distance sensor: " + String(distanceSensor.read()) + 
                  ", Motor running: " + String(feedMotor ? feedMotor->isRunning() : false) +
                  ", Motor enabled: " + String(motorsEnabled));
  }
  
  // Check for wood detection (active LOW - sensor reads 0 when wood detected)
  if (idleWoodSensor.fell()) {
    // Only start sequence if run cycle switch is active
    if (isRunCycleSwitchActive()) {
      Serial.println("WOOD DETECTED - RUN CYCLE SWITCH ACTIVE - Starting FEED TO DISTANCE sequence");
      transitionToState(STATE_FEED_TO_DISTANCE);
    } else {
      Serial.println("WOOD DETECTED - RUN CYCLE SWITCH NOT ACTIVE - Ignoring wood detection");
    }
    return;
  }
  
  // Check for run cycle switch activation (active HIGH - switch reads 1 when triggered)
  if (runCycleSwitch.rose()) {
    Serial.println("RUN CYCLE SWITCH ACTIVATED - Cutting cycle enabled");
  }
  
  // Check for run cycle switch deactivation (switch released)
  if (runCycleSwitch.fell()) {
    Serial.println("RUN CYCLE SWITCH DEACTIVATED - Cutting cycle disabled");
  }
  
  // Check for right switch activation (active HIGH - switch reads 1 when triggered)
  if (rightSwitch.rose()) {
    Serial.println("RIGHT SWITCH TRIGGERED - Starting continuous feed forward");
    startContinuousFeed();
  }
  
  // Check for right switch deactivation (switch released)
  if (rightSwitch.fell()) {
    Serial.println("RIGHT SWITCH RELEASED - Stopping continuous feed");
    stopContinuousFeed();
  }
  
  // Check for red button activation (active HIGH - button reads 1 when pressed)
  if (redButton.rose()) {
    Serial.println("RED BUTTON PRESSED - Starting continuous feed forward");
    startContinuousFeed();
  }
  
  // Check for red button deactivation (button released)
  if (redButton.fell()) {
    Serial.println("RED BUTTON RELEASED - Stopping continuous feed");
    stopContinuousFeed();
  }
  
  // Sleep mode functionality is handled by checkMotorTimeout() in main loop
}

void exitIdleState() {
  // Nothing specific needed when exiting idle state
  // Motor enable/disable is handled by the target state
} 