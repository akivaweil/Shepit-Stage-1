#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"
#include <Bounce2.h>

//* ************************************************************************
//* ******************** FEED TO DISTANCE STATE ***************************
//* ************************************************************************
// The FEED_TO_DISTANCE state feeds the wood forward until the distance sensor
// is triggered, then waits for the configured delay before starting cutting cycle

// Wood distance sensor debouncer
static Bounce2::Button distanceSensor = Bounce2::Button();

// Timing variables for the feed to distance sequence
static unsigned long feedStartTime = 0;
static bool feedMotorMoving = false;
static bool distanceSensorTriggered = false;
static unsigned long delayStartTime = 0;
static bool delayComplete = false;

void enterFeedToDistanceState() {
  // Initialize distance sensor with pulldown (active HIGH)
  distanceSensor.attach(WOOD_DISTANCE_SENSOR_PIN, INPUT_PULLDOWN);
  distanceSensor.interval(50); // 50ms debounce
  
  // Reset all sequence variables
  feedStartTime = 0;
  feedMotorMoving = false;
  distanceSensorTriggered = false;
  delayStartTime = 0;
  delayComplete = false;
  
  // Ensure motors are enabled
  enableAllMotors();
  
  // Start with clamp retracted for feed motor movement
  retractClamp();
  
  Serial.println("FEED_TO_DISTANCE: Starting feed motor to reach distance sensor");
  
  // Start feed motor movement
  if (feedMotor) {
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
    feedMotor->runForward(); // Continuous forward movement
    feedMotorMoving = true;
    feedStartTime = millis();
    Serial.println("Feed motor started - moving forward until distance sensor triggered");
  }
}

void updateFeedToDistanceState() {
  // Update distance sensor
  distanceSensor.update();
  
  // Check if distance sensor is triggered (active HIGH)
  if (distanceSensor.read() == HIGH && !distanceSensorTriggered) {
    Serial.println("DISTANCE SENSOR TRIGGERED - Stopping feed motor");
    distanceSensorTriggered = true;
    
    // Stop the feed motor
    if (feedMotor) {
      feedMotor->forceStop();
      feedMotorMoving = false;
    }
    
    // Extend clamp to secure wood
    extendClamp();
    
    // Start the delay timer
    delayStartTime = millis();
    Serial.println("Starting " + String(woodDistanceDelay) + "ms delay before cutting cycle");
  }
  
  // Check if delay is complete
  if (distanceSensorTriggered && !delayComplete && (millis() - delayStartTime >= woodDistanceDelay)) {
    delayComplete = true;
    Serial.println("Delay complete - checking conditions for cutting cycle");
    
    // Check both conditions before starting cutting cycle
    if (isRunCycleSwitchActive() && isWoodPresent()) {
      Serial.println("Conditions met - RUN CYCLE SWITCH ACTIVE & WOOD DETECTED - Starting cutting cycle");
      transitionToState(STATE_CUTTING);
    } else {
      if (!isRunCycleSwitchActive()) {
        Serial.println("RUN CYCLE SWITCH NOT ACTIVE - Canceling cutting cycle, returning to IDLE");
      } else if (!isWoodPresent()) {
        Serial.println("NO WOOD DETECTED - Canceling cutting cycle, returning to IDLE");
      }
      transitionToState(STATE_IDLE);
    }
  }
}

void exitFeedToDistanceState() {
  // Clean up state variables
  feedMotorMoving = false;
  distanceSensorTriggered = false;
  delayComplete = false;
}
