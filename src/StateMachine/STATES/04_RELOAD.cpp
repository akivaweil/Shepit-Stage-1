#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"
#include <Bounce2.h>

// External motor objects from main.cpp
extern FastAccelStepper *feedMotor;
extern FastAccelStepper *cutMotor;

//* ************************************************************************
//* ************************ RELOAD STATE *********************************
//* ************************************************************************
// The RELOAD state handles the reload mode functionality where the feed motor
// runs in reverse to move wood backward while the cut motor remains enabled
// but stationary. This allows for repositioning wood without cutting.
//
// SAFETY FEATURE: If the run cycle switch is deactivated at any point during
// this state, the machine will immediately stop the feed motor, extend the clamp
// to secure the wood, and return to the IDLE state
//
// TIMEOUT PROTECTION: The reload mode will automatically stop after 5 seconds
// if not manually stopped, and the clamp will extend to secure the wood

// Timing variables for the reload sequence
static unsigned long reloadStartTime = 0;
static bool reloadMotorMoving = false;
static bool reloadTimeoutOccurred = false;
static bool reloadMovementComplete = false;
static const unsigned long RELOAD_TIMEOUT_MS = 10000; // 10 second timeout (increased for step movement)
static const int32_t RELOAD_STEPS = 5000; // 5000 steps in reverse direction

void enterReloadState() {
  // Reset all sequence variables
  reloadStartTime = 0;
  reloadMotorMoving = false;
  reloadTimeoutOccurred = false;
  reloadMovementComplete = false;
  
  // Check if right switch is still active before proceeding
  if (digitalRead(RIGHT_SWITCH_PIN) != HIGH) {
    Serial.println("RELOAD: RIGHT SWITCH NOT ACTIVE - Returning to IDLE immediately");
    transitionToState(STATE_IDLE);
    return;
  }
  
  // Ensure motors are enabled
  enableAllMotors();
  
  // Verify motors are actually enabled
  if (!motorsEnabled) {
    Serial.println("RELOAD: WARNING - Motors may not be enabled, attempting to enable again");
    enableAllMotors();
    delay(50); // Give motors time to enable
  }
  
  Serial.println("RELOAD: Motor enable state - motorsEnabled: " + String(motorsEnabled));
  
  // Check actual motor enable pin state
  Serial.println("RELOAD: Feed motor enable pin state: " + String(digitalRead(FEED_MOTOR_ENABLE_PIN)));
  Serial.println("RELOAD: Cut motor enable pin state: " + String(digitalRead(CUT_MOTOR_ENABLE_PIN)));
  
  // Start with clamp retracted for feed motor movement
  retractClamp();
  
  Serial.println("RELOAD: Starting reload mode - feed motor reversing, cut motor enabled but stationary");
  
  // Start feed motor movement in reverse (backward)
  if (feedMotor) {
    // Validate motor configuration values
    if (feedMotorSpeed <= 0) {
      Serial.println("RELOAD: ERROR - Invalid feed motor speed: " + String(feedMotorSpeed));
      return;
    }
    if (feedMotorAcceleration <= 0) {
      Serial.println("RELOAD: ERROR - Invalid feed motor acceleration: " + String(feedMotorAcceleration));
      return;
    }
    
    Serial.println("RELOAD: Configuring feed motor - Speed: " + String(feedMotorSpeed) + "Hz, Accel: " + String(feedMotorAcceleration));
    
    // Get initial position for movement verification
    int32_t initialPosition = feedMotor->getCurrentPosition();
    Serial.println("RELOAD: Initial motor position: " + String(initialPosition));
    
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
    
    // Check motor state before starting
    Serial.println("RELOAD: Motor state before start - isRunning(): " + String(feedMotor->isRunning()));
    
    feedMotor->move(-RELOAD_STEPS); // Move 5000 steps in reverse direction
    reloadMotorMoving = true;
    reloadStartTime = millis();
    Serial.println("RELOAD: Feed motor started - moving " + String(RELOAD_STEPS) + " steps backward for reload operation");
    Serial.println("RELOAD: Timeout initialized at " + String(reloadStartTime) + "ms, will timeout at " + String(reloadStartTime + RELOAD_TIMEOUT_MS) + "ms");
    
    // Verify motor is actually running
    delay(10); // Brief delay to let motor start
    if (feedMotor->isRunning()) {
      Serial.println("RELOAD: Motor verification - feedMotor->isRunning() = TRUE");
      
      // Check if position actually changed (motor is moving)
      delay(100); // Wait a bit longer to see movement
      int32_t newPosition = feedMotor->getCurrentPosition();
      Serial.println("RELOAD: Motor position after 100ms: " + String(newPosition));
      if (newPosition == initialPosition) {
        Serial.println("RELOAD: WARNING - Motor position unchanged - motor may not be moving despite isRunning() = TRUE");
      } else {
        Serial.println("RELOAD: Motor movement confirmed - position changed from " + String(initialPosition) + " to " + String(newPosition));
      }
    } else {
      Serial.println("RELOAD: WARNING - Motor verification - feedMotor->isRunning() = FALSE");
      Serial.println("RELOAD: Motor may not be running despite start command");
      Serial.println("RELOAD: Check motor enable state and connections");
    }
  } else {
    Serial.println("RELOAD: ERROR - feedMotor pointer is NULL");
  }
}

void updateReloadState() {
  //! ************************************************************************
  //! SAFETY CHECK: RIGHT SWITCH DEACTIVATION DETECTION
  //! ************************************************************************
  // Check if right switch is turned off during operation - return to idle immediately
  if (digitalRead(RIGHT_SWITCH_PIN) != HIGH) {
    Serial.println("RELOAD: RIGHT SWITCH TURNED OFF - Stopping operation and returning to IDLE");
    
    // Stop the feed motor immediately
    if (feedMotor && feedMotor->isRunning()) {
      Serial.println("RELOAD: Stopping feed motor due to run cycle switch OFF");
      feedMotor->forceStop();
      
      // Verify motor actually stopped
      delay(10);
      if (feedMotor->isRunning()) {
        Serial.println("RELOAD: WARNING - Motor still running after forceStop(), trying again");
        feedMotor->forceStop();
        delay(10);
        if (feedMotor->isRunning()) {
          Serial.println("RELOAD: ERROR - Motor still running after multiple stop attempts");
        } else {
          Serial.println("RELOAD: Motor stopped successfully on second attempt");
        }
      } else {
        Serial.println("RELOAD: Motor stopped successfully");
      }
      
      reloadMotorMoving = false;
    }
    
    // Extend clamp to secure wood in current position
    extendClamp();
    
    // Return to idle state
    transitionToState(STATE_IDLE);
    return;
  }
  
  //! ************************************************************************
  //! TIMEOUT DETECTION AND HANDLING (10 SECOND LIMIT)
  //! ************************************************************************
  // Check for reload mode timeout (10 seconds) - if motor runs too long, go to idle
  if (reloadMotorMoving && !reloadTimeoutOccurred && !reloadMovementComplete) {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - reloadStartTime;
    
    // Check for timeout every 1000ms for step movement
    if (elapsedTime % 1000 == 0) {
      // Debug timeout progress
      Serial.println("RELOAD: Timeout progress - " + String(elapsedTime) + "ms elapsed, " + String(RELOAD_TIMEOUT_MS - elapsedTime) + "ms remaining");
    }
    
    if (elapsedTime >= RELOAD_TIMEOUT_MS) {
      reloadTimeoutOccurred = true;
      Serial.println("RELOAD: TIMEOUT DETECTED at " + String(elapsedTime) + "ms");
      Serial.println("RELOAD: Timeout variables - reloadMotorMoving: " + String(reloadMotorMoving) + ", reloadTimeoutOccurred: " + String(reloadTimeoutOccurred));
    }
  }
  
  // Handle timeout when it occurs - stop motor and extend clamp immediately
  if (reloadTimeoutOccurred) {
    Serial.println("RELOAD: TIMEOUT - Reload mode ran for 10 seconds without completion");
    Serial.println("RELOAD: Timeout trigger details:");
    Serial.println("RELOAD:   - reloadTimeoutOccurred: " + String(reloadTimeoutOccurred));
    Serial.println("RELOAD:   - reloadMotorMoving: " + String(reloadMotorMoving));
    Serial.println("RELOAD:   - feedMotor exists: " + String(feedMotor ? "YES" : "NO"));
    Serial.println("RELOAD:   - feedMotor->isRunning(): " + String(feedMotor ? (feedMotor->isRunning() ? "TRUE" : "FALSE") : "N/A"));
    Serial.println("RELOAD:   - elapsed time: " + String(millis() - reloadStartTime) + "ms");
    Serial.println("RELOAD: Returning to IDLE - cycle switch must be flipped OFF and ON to reset");
    
    // Stop the feed motor immediately
    if (feedMotor && feedMotor->isRunning()) {
      Serial.println("RELOAD: Stopping feed motor due to timeout");
      feedMotor->forceStop();
      
      // Verify motor actually stopped
      delay(10);
      if (feedMotor->isRunning()) {
        Serial.println("RELOAD: WARNING - Motor still running after forceStop(), trying again");
        feedMotor->forceStop();
        delay(10);
        if (feedMotor->isRunning()) {
          Serial.println("RELOAD: ERROR - Motor still running after multiple stop attempts");
        } else {
          Serial.println("RELOAD: Motor stopped successfully on second attempt");
        }
      } else {
        Serial.println("RELOAD: Motor stopped successfully");
      }
      
      reloadMotorMoving = false;
    }
    
    // Extend clamp to secure wood in current position
    extendClamp();
    
    // Return to idle state
    transitionToState(STATE_IDLE);
    return;
  }
  
  //! ************************************************************************
  //! DEBUG LOGGING FOR TIMEOUT MONITORING
  //! ************************************************************************
  // Debug logging for timeout troubleshooting
  if (reloadMotorMoving && !reloadTimeoutOccurred && !reloadMovementComplete) {
    unsigned long elapsedTime = millis() - reloadStartTime;
    static unsigned long lastDebugTime = 0;
    
    // Log every 1000ms for better debugging
    if (elapsedTime - lastDebugTime >= 1000) {
      Serial.println("RELOAD: Motor running for " + String(elapsedTime) + "ms, timeout at " + String(RELOAD_TIMEOUT_MS) + "ms");
      Serial.println("RELOAD: Debug - reloadMotorMoving: " + String(reloadMotorMoving));
      if (feedMotor) {
        Serial.println("RELOAD: Debug - feedMotor->isRunning(): " + String(feedMotor->isRunning()));
        // Additional motor state verification
        if (!feedMotor->isRunning()) {
          Serial.println("RELOAD: WARNING - Motor stopped unexpectedly at " + String(elapsedTime) + "ms");
          reloadMotorMoving = false; // Update flag to match actual state
        }
      }
      lastDebugTime = elapsedTime;
    }
    
    // Additional debug info when approaching timeout
    if (elapsedTime >= 4000 && elapsedTime < RELOAD_TIMEOUT_MS) {
      Serial.println("RELOAD: WARNING - Approaching timeout in " + String(RELOAD_TIMEOUT_MS - elapsedTime) + "ms");
    }
  }
  
  //! ************************************************************************
  //! MOVEMENT COMPLETION DETECTION
  //! ************************************************************************
  // Check if the 5000-step movement is complete
  if (reloadMotorMoving && !reloadMovementComplete && feedMotor && !feedMotor->isRunning()) {
    reloadMovementComplete = true;
    reloadMotorMoving = false;
    
    Serial.println("RELOAD: Movement complete - " + String(RELOAD_STEPS) + " steps completed");
    Serial.println("RELOAD: Extending clamp to secure wood in new position");
    
    // Extend clamp to secure wood in new position
    extendClamp();
    
    // Return to idle state after successful completion
    Serial.println("RELOAD: Reload operation successful - returning to IDLE state");
    transitionToState(STATE_IDLE);
    return;
  }
  
  // Note: The reload state will continue running until:
  // 1. Run cycle switch is turned off (safety stop)
  // 2. Timeout occurs (10 seconds)
  // 3. Movement completes (5000 steps)
  // 4. Manual transition to another state via serial command
  // The state machine will continue calling this function until one of these conditions is met
}

void exitReloadState() {
  // Clean up state variables
  Serial.println("RELOAD: Exiting state - cleaning up variables");
  Serial.println("RELOAD: Exit state - reloadMotorMoving: " + String(reloadMotorMoving) + ", reloadTimeoutOccurred: " + String(reloadTimeoutOccurred) + ", reloadMovementComplete: " + String(reloadMovementComplete));
  
  reloadMotorMoving = false;
  reloadTimeoutOccurred = false;
  reloadMovementComplete = false;
  
  Serial.println("RELOAD: Exit state - variables reset");
}
