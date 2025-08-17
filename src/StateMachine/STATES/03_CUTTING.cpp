#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"
#include <Bounce2.h>

//* ************************************************************************
//* ************************ CUTTING STATE ********************************
//* ************************************************************************
// The CUTTING state now includes a positioning step at the beginning of each cycle:
// 1. Feed wood forward until distance sensor is triggered (positioning)
// 2. Perform the actual cutting operation
// 3. This ensures wood is always in the correct position before each cut
// 4. Each cutting cycle completes fully before checking run cycle switch status
// 5. No mid-cycle interruptions - complete cycle or complete cancellation

// Wood distance sensor debouncer for positioning
static Bounce2::Button distanceSensor = Bounce2::Button();

// Cutting cycle phases
enum CuttingCyclePhase {
  PHASE_POSITIONING,    // Phase 1: Feed wood to correct position
  PHASE_CUTTING,        // Phase 2: Perform actual cutting
  PHASE_COMPLETE        // Phase 3: Cutting complete
};

static CuttingCyclePhase currentPhase = PHASE_POSITIONING;
static bool feedMotorStarted = false;
static bool cutMotorStarted = false;
static bool distanceSensorTriggered = false;
static unsigned long positioningStartTime = 0;
static unsigned long delayStartTime = 0;
static bool delayComplete = false;

void enterCuttingState() {
  // Check both conditions at the beginning of each cutting cycle
  if (!isRunCycleSwitchActive() || !isWoodPresent()) {
    if (!isRunCycleSwitchActive()) {
      Serial.println("CUTTING: RUN CYCLE SWITCH NOT ACTIVE - Canceling cutting cycle");
    } else if (!isWoodPresent()) {
      Serial.println("CUTTING: NO WOOD DETECTED - Canceling cutting cycle");
    }
    
    // Return to idle state
    transitionToState(STATE_IDLE);
    return;
  }
  
  // Conditions met - proceed with cutting cycle
  Serial.println("CUTTING: Conditions verified - RUN CYCLE SWITCH ACTIVE & WOOD DETECTED");
  Serial.println("CUTTING: Starting positioning phase - feeding wood to correct position");
  
  // Initialize distance sensor for positioning
  distanceSensor.attach(WOOD_DISTANCE_SENSOR_PIN, INPUT);
  distanceSensor.interval(50); // 50ms debounce
  
  // Enable motors with delay to ensure proper wake-up from sleep mode
  enableAllMotorsWithDelay();
  
  // Reset all phase variables
  currentPhase = PHASE_POSITIONING;
  feedMotorStarted = false;
  cutMotorStarted = false;
  distanceSensorTriggered = false;
  positioningStartTime = 0;
  delayStartTime = 0;
  delayComplete = false;
  
  // Start with clamp retracted for feed motor movement during positioning
  retractClamp();
  
  // Ensure feed motor is stopped before starting cutting cycle
  if (feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
    Serial.println("CUTTING: Stopped feed motor before starting cutting cycle");
  }
}

void updateCuttingState() {
  // Update distance sensor
  distanceSensor.update();
  
  // Only check conditions at the beginning of the cycle or when transitioning between phases
  // This ensures each cutting cycle completes fully before checking run cycle switch status
  
  // Wait for motor enable delay to complete before starting any movement
  if (!isMotorEnableDelayComplete()) {
    return; // Still waiting for motor stabilization
  }
  
  // Handle different phases of the cutting cycle
  switch (currentPhase) {
    case PHASE_POSITIONING:
      updatePositioningPhase();
      break;
      
    case PHASE_CUTTING:
      updateCuttingPhase();
      break;
      
    case PHASE_COMPLETE:
      // Cutting cycle complete - NOW check conditions before transitioning
      if (!isRunCycleSwitchActive() || !isWoodPresent()) {
        if (!isRunCycleSwitchActive()) {
          Serial.println("CUTTING: RUN CYCLE SWITCH NOT ACTIVE - Canceling cycle, returning to IDLE");
        } else if (!isWoodPresent()) {
          Serial.println("CUTTING: NO WOOD DETECTED - Canceling cycle, returning to IDLE");
        }
        
        // Stop all motors if they're running
        if (feedMotor && feedMotor->isRunning()) {
          feedMotor->forceStop();
        }
        if (cutMotor && cutMotor->isRunning()) {
          cutMotor->forceStop();
        }
        
        // Return to idle state
        transitionToState(STATE_IDLE);
        return;
      }
      
      // Conditions still met - continue with returning state
      Serial.println("CUTTING: Cutting cycle complete - transitioning to RETURNING state");
      transitionToState(STATE_RETURNING);
      break;
  }
}

//* ************************************************************************
//* *********************** POSITIONING PHASE *****************************
//* ************************************************************************
// Phase 1: Feed wood forward until distance sensor is triggered
// This ensures wood is always in the correct position before cutting
// Note: No condition checking during positioning - cycle must complete

void updatePositioningPhase() {
  // Start feed motor if not already started
  if (!feedMotorStarted && feedMotor) {
    Serial.println("CUTTING: Starting feed motor for positioning phase");
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
    feedMotor->runForward(); // Continuous forward movement
    feedMotorStarted = true;
    positioningStartTime = millis();
  }
  
  // Check if distance sensor is triggered (active HIGH - HIGH when wood detected)
  if (distanceSensor.read() == HIGH && !distanceSensorTriggered) {
    Serial.println("CUTTING: DISTANCE SENSOR TRIGGERED - Positioning complete");
    distanceSensorTriggered = true;
    
    // Stop the feed motor
    if (feedMotor) {
      feedMotor->forceStop();
      feedMotorStarted = false;
    }
    
    // Extend clamp to secure wood in position
    extendClamp();
    
    // Start the delay timer before cutting
    delayStartTime = millis();
    Serial.println("CUTTING: Starting " + String(woodDistanceDelay) + "ms delay before cutting operation");
  }
  
  // Check if delay is complete
  if (distanceSensorTriggered && !delayComplete && (millis() - delayStartTime >= woodDistanceDelay)) {
    delayComplete = true;
    Serial.println("CUTTING: Delay complete - transitioning to cutting phase");
    
    // Ensure feed motor is completely stopped before cutting
    if (feedMotor && feedMotor->isRunning()) {
      feedMotor->forceStop();
      Serial.println("CUTTING: Feed motor stopped before cutting phase");
    }
    
    // Ensure clamp is engaged to secure wood during cutting
    if (isClampRetracted()) {
      extendClamp();
      Serial.println("CUTTING: Clamp engaged to secure wood for cutting");
    }
    
    // Move to cutting phase - positioning phase complete
    currentPhase = PHASE_CUTTING;
  }
}

//* ************************************************************************
//* ************************ CUTTING PHASE ********************************
//* ************************************************************************
// Phase 2: Perform the actual cutting operation
// Wood is already positioned and secured by clamp
// Note: No condition checking during cutting - cycle must complete

void updateCuttingPhase() {
  // Start cut motor movement if not already started
  if (!cutMotorStarted && cutMotor) {
    Serial.println("CUTTING: Starting cut motor forward movement (" + String(cutMotorSteps) + " steps)");
    cutMotor->move(cutMotorSteps);
    cutMotorStarted = true;
  }
  
  // Check if cutting movement is complete
  if (cutMotorStarted && cutMotor && !cutMotor->isRunning()) {
    Serial.println("CUTTING: Cut motor forward movement COMPLETE");
    
    // Cutting operation complete - move to completion phase
    currentPhase = PHASE_COMPLETE;
  }
}

void exitCuttingState() {
  // Reset all phase variables for next cycle
  currentPhase = PHASE_POSITIONING;
  feedMotorStarted = false;
  cutMotorStarted = false;
  distanceSensorTriggered = false;
  positioningStartTime = 0;
  delayStartTime = 0;
  delayComplete = false;
  
  // Motors stay enabled for the next state
  // No need to disable motors here
} 