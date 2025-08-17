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
// Distance sensor continuously controls feed motor (active LOW - runs when triggered)

// Wood sensor debouncer for IDLE state monitoring
static Bounce2::Button idleWoodSensor = Bounce2::Button();

// Run cycle switch debouncer for cutting cycle control
static Bounce2::Button runCycleSwitch = Bounce2::Button();

// Right switch debouncer for continuous feed operation
static Bounce2::Button rightSwitch = Bounce2::Button();

// Red button debouncer for continuous feed operation
static Bounce2::Button redButton = Bounce2::Button();

// Distance sensor debouncer for continuous feed motor control
static Bounce2::Button distanceSensor = Bounce2::Button();

void enterIdleState() {
  // Initialize wood sensor monitoring
  idleWoodSensor.attach(WOOD_PRESENT_SENSOR_PIN, INPUT);
  idleWoodSensor.interval(50); // 50ms debounce
  
  // Initialize run cycle switch monitoring
  runCycleSwitch.attach(RUN_CYCLE_SWITCH_PIN, INPUT);
  runCycleSwitch.interval(50); // 50ms debounce
  
  // Initialize right switch monitoring
  rightSwitch.attach(RIGHT_SWITCH_PIN, INPUT);
  rightSwitch.interval(50); // 50ms debounce
  
  // Initialize red button monitoring
  redButton.attach(RED_BUTTON_PIN, INPUT);
  redButton.interval(50); // 50ms debounce
  
  // Initialize distance sensor monitoring for continuous feed motor control
  distanceSensor.attach(WOOD_DISTANCE_SENSOR_PIN, INPUT);
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
  
  // Initialize distance sensor feed motor control
  Serial.println("Distance sensor feed motor control initialized - motor runs when sensor triggered (HIGH)");
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
  
  //! ************************************************************************
  //! DISTANCE SENSOR FEED MOTOR CONTROL (ACTIVE HIGH)
  //! ************************************************************************
  // Distance sensor triggers feed motor continuously while triggered
  // Sensor reads HIGH (1) when wood detected - motor runs
  // Sensor reads LOW (0) when no wood - motor stops
  
  if (distanceSensor.read() == HIGH) {
    // Sensor triggered (HIGH) - wood detected, run feed motor continuously
    if (feedMotor && !feedMotor->isRunning()) {
      // Retract clamp before feed motor movement
      retractClamp();
      
      // Configure and start feed motor
      feedMotor->setSpeedInHz(feedMotorSpeed);
      feedMotor->setAcceleration(feedMotorAcceleration);
      feedMotor->runForward();
      
      // Reset motor timeout to keep motors enabled
      resetMotorTimeout();
    }
  } else {
    // Sensor not triggered (LOW) - no wood detected, stop feed motor
    if (feedMotor && feedMotor->isRunning()) {
      // Stop the feed motor
      feedMotor->forceStop();
      
      // Extend clamp when feed motor stops
      extendClamp();
    }
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