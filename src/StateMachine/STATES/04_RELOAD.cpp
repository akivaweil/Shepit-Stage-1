#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"
#include <Bounce2.h>

// External motor objects from main.cpp
extern FastAccelStepper *feedMotor;
extern FastAccelStepper *cutMotor;

// SIMPLIFIED: Only essential timing and state variables
static unsigned long reloadStartTime = 0;
static bool reloadMotorMoving = false;
static const unsigned long RELOAD_TIMEOUT_MS = 10000; // 10 second timeout
static const int32_t RELOAD_STEPS = 5000; // 5000 steps in reverse direction

void enterReloadState() {
  // Reset feed motor timeout lock when entering this state
  // This ensures the lock is cleared regardless of which state we came from
  if (isFeedMotorTimeoutLocked()) {
    Serial.println("RELOAD: Resetting feed motor timeout lock from previous state");
    resetFeedMotorTimeoutLock();
  }
  
  // CRITICAL FIX: Reset feed motor control variables to ensure clean start
  // This prevents issues with feed motor control from previous states
  resetFeedMotorControlVariables();
  
  // Reset all sequence variables
  reloadStartTime = 0;
  reloadMotorMoving = false;
  
  // Check if right switch is still active before proceeding
  if (digitalRead(RELOAD_SWITCH_PIN) != HIGH) {
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
  
  // Start with clamp retracted for feed motor movement
  retractClamp();
  
  Serial.println("RELOAD: Starting reload mode - feed motor reversing");
  
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
    
    // Get initial position for movement verification
    int32_t initialPosition = feedMotor->getCurrentPosition();
    
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
    
    feedMotor->move(-RELOAD_STEPS); // Move 5000 steps in reverse direction
    reloadMotorMoving = true;
    reloadStartTime = millis();
    Serial.println("RELOAD: Feed motor started - moving " + String(RELOAD_STEPS) + " steps backward");
    
    // Verify motor is actually running
    delay(10); // Brief delay to let motor start
    if (feedMotor->isRunning()) {
      // Check if position actually changed (motor is moving)
      delay(100); // Wait a bit longer to see movement
      int32_t newPosition = feedMotor->getCurrentPosition();
      if (newPosition == initialPosition) {
        Serial.println("RELOAD: WARNING - Motor position unchanged - motor may not be moving");
      } else {
        Serial.println("RELOAD: Motor movement confirmed - position changed from " + String(initialPosition) + " to " + String(newPosition));
      }
    } else {
      Serial.println("RELOAD: WARNING - Motor not running despite start command");
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
  bool rightSwitchActive = digitalRead(RELOAD_SWITCH_PIN) == HIGH;
  
  // Debug logging for switch state monitoring
  static bool lastRightSwitchState = false;
  if (rightSwitchActive != lastRightSwitchState) {
    Serial.println("Reload switch: " + String(rightSwitchActive ? "ACTIVE" : "INACTIVE"));
    lastRightSwitchState = rightSwitchActive;
  }
  
  if (!rightSwitchActive) {
    Serial.println("RELOAD: Right switch turned OFF - stopping operation");
    
    // Stop the feed motor immediately
    if (feedMotor && feedMotor->isRunning()) {
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
  if (reloadMotorMoving) {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - reloadStartTime;
    
    // Check for timeout every 1000ms for step movement
    if (elapsedTime % 1000 == 0) {
      // Debug timeout progress
      // Reduced periodic timeout log
      Serial.println("RELOAD: Timeout " + String(elapsedTime) + "/" + String(RELOAD_TIMEOUT_MS) + "ms");
    }
    
    if (elapsedTime >= RELOAD_TIMEOUT_MS) {
      Serial.println("RELOAD: TIMEOUT DETECTED at " + String(elapsedTime) + "ms");
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
        }
        
        reloadMotorMoving = false;
      }
      
      // Extend clamp to secure wood in current position
      extendClamp();
      
      // Return to idle state
      transitionToState(STATE_IDLE);
      return;
    }
  }
  
  //! ************************************************************************
  //! DEBUG LOGGING FOR TIMEOUT MONITORING
  //! ************************************************************************
  // Debug logging for timeout troubleshooting
  if (reloadMotorMoving) {
    unsigned long elapsedTime = millis() - reloadStartTime;
    static unsigned long lastDebugTime = 0;
    
    // Log every 1000ms for better debugging
    if (elapsedTime - lastDebugTime >= 1000) {
      // Reduced periodic runtime log
      Serial.println("RELOAD: Running " + String(elapsedTime) + "/" + String(RELOAD_TIMEOUT_MS) + "ms");
      if (feedMotor) {
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
      // Keep concise
      Serial.println("RELOAD: Approaching timeout");
    }
  }
  
  //! ************************************************************************
  //! MOVEMENT COMPLETION DETECTION
  //! ************************************************************************
  // Check if the 5000-step movement is complete
  if (reloadMotorMoving && feedMotor && !feedMotor->isRunning()) {
    reloadMotorMoving = false;
    
    Serial.println("RELOAD: Movement complete - " + String(RELOAD_STEPS) + " steps completed");
    
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
  
  reloadMotorMoving = false;
}
