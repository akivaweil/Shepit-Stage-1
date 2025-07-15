#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"
#include <Bounce2.h>

//* ************************************************************************
//* ************************ RELOADING ************************************
//* ************************************************************************
// The RELOADING state monitors the wood sensor and controls the reload sequence:
// 1. When wood is detected (LOW signal), enable cut motor for 3 seconds
// 2. After 3 seconds, enable feed motor and move feedMotorSteps
// 3. Begin cutting cycle

// Wood sensor debouncer
static Bounce2::Button woodSensor = Bounce2::Button();

// Timing variables for the reload sequence
static unsigned long cutMotorStartTime = 0;
static unsigned long feedMotorStartTime = 0;
static bool cutMotorRunning = false;
static bool feedMotorMoving = false;
static bool woodDetected = false;
static bool sequenceStarted = false;

void enterReloadingState() {
  // Initialize wood sensor with pullup (active LOW)
  woodSensor.attach(IS_WOOD_PIN, INPUT_PULLUP);
  woodSensor.interval(50); // 50ms debounce
  
  // Reset all sequence variables
  cutMotorStartTime = 0;
  feedMotorStartTime = 0;
  cutMotorRunning = false;
  feedMotorMoving = false;
  woodDetected = false;
  sequenceStarted = false;
  
  // Ensure motors are disabled initially
  disableFeedMotor();
  disableCutMotor();
  
  // Start with clamp extended (ready position)
  extendClamp();
  
  Serial.println("RELOADING: Wood detection active");
}

void updateReloadingState() {
  // Update wood sensor
  woodSensor.update();
  
  // Check for wood detection (active LOW)
  if (woodSensor.read() == LOW && !sequenceStarted) {
    Serial.println("WOOD DETECTED - Starting sequence");
    // Wood detected - start the sequence
    woodDetected = true;
    sequenceStarted = true;
    cutMotorStartTime = millis();
    
    // Enable cut motor only
    enableCutMotor();
    disableFeedMotor(); // Ensure feed motor stays disabled
    cutMotorRunning = true;
  }
  
  // Handle cut motor timing (3 second duration)
  if (cutMotorRunning && (millis() - cutMotorStartTime >= 3000)) {
    // 3 seconds elapsed - enable feed motor and start movement
    enableFeedMotor();
    cutMotorRunning = false;
    feedMotorStartTime = millis();
    
    // Retract clamp before feed motor movement
    retractClamp();
    
    // Small delay to allow motor driver to stabilize after enable
    delay(100);
    
    // Configure and move feed motor
    if (feedMotor) {
      feedMotor->setSpeedInHz(feedMotorSpeed);
      feedMotor->setAcceleration(feedMotorAcceleration);
      feedMotor->move(feedMotorSteps);
      feedMotorMoving = true;
      Serial.println("Feed motor moving " + String(feedMotorSteps) + " steps");
    }
  }
  
  // Check if feed motor movement is complete
  if (feedMotorMoving && feedMotor && !feedMotor->isRunning()) {
    // Feed motor movement complete - extend clamp and start cutting cycle
    extendClamp();
    feedMotorMoving = false;
    
    Serial.println("Feed complete - Starting cutting cycle");
    
    // Reset activity timer and transition to cutting state
    resetMotorTimeout();
    transitionToState(STATE_CUTTING);
  }
}

void exitReloadingState() {
  // Clean up state variables
  cutMotorRunning = false;
  feedMotorMoving = false;
  woodDetected = false;
  sequenceStarted = false;
} 