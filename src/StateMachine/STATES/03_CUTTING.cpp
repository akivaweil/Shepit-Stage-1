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
// 6. IMPORTANT: Run cycle switch is only checked at beginning and end - no mid-cycle stops

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
static bool cycleStarted = false; // Track if cycle has begun to prevent mid-cycle interruption

// Feed motor timeout tracking for 2-second safety limit during positioning
static bool feedMotorTimeoutOccurred = false;

// Flag to track if wood needs to be moved away from sensor
static bool needToMoveWoodAway = false;

void enterCuttingState() {
  // Check only wood presence at the beginning
  // Run cycle switch is NOT checked here - once cutting cycle starts, it completes
  if (!isWoodPresent()) {
    Serial.println("CUTTING: NO WOOD DETECTED - Canceling cutting cycle");
    transitionToState(STATE_IDLE);
    return;
  }
  
  // Wood present - proceed with cutting cycle (regardless of run cycle switch)
  Serial.println("CUTTING: Wood detected - starting cutting cycle");
  Serial.println("CUTTING: Starting positioning phase - feeding wood to correct position");
  Serial.println("CUTTING: NOTE: Cycle will complete regardless of cycle switch state during operation");
  
  // Mark cycle as started - no more cycle switch checks until completion
  cycleStarted = true;
  
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
  
  // Reset feed motor timeout tracking
  feedMotorTimeoutOccurred = false;
  
  // Start with clamp retracted for feed motor movement during positioning
  retractClamp();
  
  // Ensure feed motor is stopped before starting cutting cycle
  if (feedMotor && feedMotor->isRunning()) {
    feedMotor->forceStop();
    Serial.println("CUTTING: Stopped feed motor before starting cutting cycle");
  }
  
  // If distance sensor is already HIGH, we need to move wood away first
  // But we'll check this AFTER the motor movement, not before
  bool needToMoveWoodAway = false;
  distanceSensor.update();
  if (distanceSensor.read() == HIGH) {
    needToMoveWoodAway = true;
    Serial.println("CUTTING: Distance sensor already HIGH - will move wood away after motor starts");
  }
}

void updateCuttingState() {
  // Update distance sensor
  distanceSensor.update();
  
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
      // Note: Once cutting cycle starts, it completes regardless of cycle switch state
      // E-stop handles emergency situations, so we finish the current operation
      if (!isWoodPresent()) {
        Serial.println("CUTTING: NO WOOD DETECTED - Canceling cycle, returning to IDLE");
        
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
      
      // Wood still present - continue with returning state (regardless of cycle switch)
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
// IMPORTANT: Run cycle switch is ignored during positioning phase

void updatePositioningPhase() {
  // Start feed motor if not already started AND distance sensor not yet triggered
  if (!feedMotorStarted && !distanceSensorTriggered && feedMotor) {
    Serial.println("CUTTING: Starting feed motor for positioning phase");
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
    
          // Check if we need to move wood away first (sensor was HIGH when entering state)
      static bool woodMovedAway = false;
      if (needToMoveWoodAway && !woodMovedAway) {
        Serial.println("CUTTING: Moving wood away from sensor first (backward movement)");
        feedMotor->move(-FM_preCutPullback); // Move wood away using config value
        
        // Wait for movement to complete
        while (feedMotor->isRunning()) {
          delay(10);
        }
        
        // Update sensor after movement
        delay(50); // Give sensor time to settle
        distanceSensor.update();
        Serial.println("CUTTING: After moving wood away, sensor state: " + String(distanceSensor.read()));
        woodMovedAway = true;
      }
    
    // Now start forward movement for positioning
    feedMotor->runForward(); // Continuous forward movement
    feedMotorStarted = true;
    positioningStartTime = millis();
  }
  
  // Check for feed motor timeout (2 seconds) during positioning phase
  if (feedMotorStarted && !distanceSensorTriggered && !feedMotorTimeoutOccurred) {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - positioningStartTime;
    
    if (elapsedTime >= 2000) {
      feedMotorTimeoutOccurred = true;
      Serial.println("CUTTING: FEED MOTOR TIMEOUT - Motor running for 2+ seconds during positioning, stopping for safety");
      
      // Stop the feed motor immediately
      if (feedMotor) {
        feedMotor->forceStop();
        feedMotorStarted = false;
      }
      
      // Extend clamp to secure wood
      extendClamp();
      
      // Return to IDLE state due to timeout
      Serial.println("CUTTING: Returning to IDLE due to feed motor timeout");
      transitionToState(STATE_IDLE);
      return;
    }
  }
  
  // Debug: Log distance sensor state every 500ms during positioning
  static unsigned long lastSensorDebug = 0;
  if (feedMotorStarted && !distanceSensorTriggered && (millis() - lastSensorDebug >= 500)) {
    Serial.println("CUTTING: Distance sensor debug - State: " + String(distanceSensor.read()) + " (HIGH=wood detected, LOW=no wood)");
    lastSensorDebug = millis();
  }
  
  // Check if distance sensor is triggered (active HIGH - HIGH when wood detected)
  if (distanceSensor.read() == HIGH && !distanceSensorTriggered) {
    Serial.println("CUTTING: DISTANCE SENSOR TRIGGERED - Positioning complete");
    distanceSensorTriggered = true;
    
    // Stop the feed motor immediately and ensure it's stopped
    if (feedMotor) {
      feedMotor->forceStop();
      // Additional stop command to ensure motor stops
      feedMotor->stopMove();
      feedMotorStarted = false;
      
      // Wait a brief moment to ensure motor has stopped
      delay(10);
      
      // Double-check if motor is still running and force stop again if needed
      if (feedMotor->isRunning()) {
        Serial.println("CUTTING: Feed motor still running - forcing stop again");
        feedMotor->forceStop();
        delay(5);
      }
      
      Serial.println("CUTTING: Feed motor stopped successfully");
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
      Serial.println("CUTTING: Feed motor still running - forcing stop before cutting phase");
      feedMotor->forceStop();
      feedMotor->stopMove();
      delay(10);
      
      // Final check to ensure motor is stopped
      if (feedMotor->isRunning()) {
        Serial.println("CUTTING: WARNING - Feed motor still running after multiple stop attempts");
        feedMotor->forceStop();
      }
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
// IMPORTANT: Run cycle switch is ignored during cutting phase

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
  // Ensure feed motor is completely stopped before exiting
  if (feedMotor && feedMotor->isRunning()) {
    Serial.println("CUTTING: Exit - stopping feed motor");
    feedMotor->forceStop();
    feedMotor->stopMove();
    delay(10);
    
    // Final check to ensure motor is stopped
    if (feedMotor->isRunning()) {
      Serial.println("CUTTING: Exit - forcing stop again");
      feedMotor->forceStop();
    }
  }
  
  // Reset all phase variables for next cycle
  currentPhase = PHASE_POSITIONING;
  feedMotorStarted = false;
  cutMotorStarted = false;
  distanceSensorTriggered = false;
  positioningStartTime = 0;
  delayStartTime = 0;
  delayComplete = false;
  cycleStarted = false;
  
  // Motors stay enabled for the next state
  // No need to disable motors here
} 