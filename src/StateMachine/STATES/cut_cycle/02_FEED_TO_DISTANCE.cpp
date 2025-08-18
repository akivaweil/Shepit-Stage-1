#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"
#include <Bounce2.h>

//* ************************************************************************
//* ******************** FEED TO DISTANCE STATE ***************************
//* ************************************************************************
// The FEED_TO_DISTANCE state feeds the wood forward until the distance sensor
// is triggered, then waits for the configured delay before starting cutting cycle
// 
// SAFETY FEATURE: If the run cycle switch is deactivated at any point during
// this state, the machine will immediately stop the feed motor, extend the clamp
// to secure the wood, and return to the IDLE state
//
// TIMEOUT PROTECTION: The feed motor will automatically stop after 2 seconds
// if the distance sensor is not triggered, and the clamp will extend to secure
// the wood before returning to IDLE state

// Wood distance sensor debouncer
static Bounce2::Button feedDistanceSensor = Bounce2::Button();

// Wood presence sensor debouncer
static Bounce2::Button woodPresenceSensor = Bounce2::Button();

// Timing variables for the feed to distance sequence
static unsigned long feedStartTime = 0;
static bool feedMotorMoving = false;
static bool distanceSensorTriggered = false;
static unsigned long delayStartTime = 0;
static bool delayComplete = false;
static bool timeoutOccurred = false;

// SIMPLIFIED: Remove unnecessary wood detection tracking
// The system already knows wood is present when entering this state

void enterFeedToDistanceState() {
  // Initialize distance sensor with INPUT mode (active HIGH - HIGH when wood detected)
  feedDistanceSensor.attach(WOOD_DISTANCE_SENSOR_PIN, INPUT);
  feedDistanceSensor.interval(distanceSensorDebounceTime); // Distance sensor debounce
  
  // Initialize wood presence sensor with proper debouncing
  woodPresenceSensor.attach(WOOD_PRESENT_SENSOR_PIN, INPUT);
  woodPresenceSensor.interval(distanceSensorDebounceTime); // Use same debounce time
  
  // CRITICAL FIX: Reset feed motor control variables to ensure clean start
  // This prevents issues with feed motor control from previous states
  resetFeedMotorControlVariables();
  
  // Reset all sequence variables
  feedStartTime = 0;
  feedMotorMoving = false;
  distanceSensorTriggered = false;
  delayStartTime = 0;
  delayComplete = false;
  timeoutOccurred = false;
  
  // Check if run cycle switch is still active before proceeding
  if (!isRunCycleSwitchActive()) {
    Serial.println("FEED_TO_DISTANCE: RUN CYCLE SWITCH NOT ACTIVE - Returning to IDLE immediately");
    transitionToState(STATE_IDLE);
    return;
  }
  
  // CRITICAL FIX: Reset feed motor timeout lock AND local timeout variables when entering this state
  // This ensures both the global lock and local timeout flags are cleared
  if (isFeedMotorTimeoutLocked()) {
    Serial.println("FEED_TO_DISTANCE: Resetting feed motor timeout lock from previous state");
    resetFeedMotorTimeoutLock();
  }
  
  // CRITICAL FIX: Also reset all feed motor control variables to ensure clean state
  // This clears the local feedMotorTimeoutOccurred variable in the IDLE state
  resetFeedMotorControlVariables();
  
  // Ensure motors are enabled
  enableAllMotors();
  
  // Verify motors are actually enabled
  if (!motorsEnabled) {
    Serial.println("FEED_TO_DISTANCE: WARNING - Motors may not be enabled, attempting to enable again");
    enableAllMotors();
    delay(50); // Give motors time to enable
  }
  
  // Start with clamp retracted for feed motor movement
  retractClamp();
  
  Serial.println("FEED_TO_DISTANCE: Starting feed motor to reach distance sensor");
  
  // Start feed motor movement
  if (feedMotor) {
    // Validate motor configuration values
    if (feedMotorSpeed <= 0) {
      Serial.println("FEED_TO_DISTANCE: ERROR - Invalid feed motor speed: " + String(feedMotorSpeed));
      return;
    }
    if (feedMotorAcceleration <= 0) {
      Serial.println("FEED_TO_DISTANCE: ERROR - Invalid feed motor acceleration: " + String(feedMotorAcceleration));
      return;
    }
    
    // Get initial position for movement verification
    int32_t initialPosition = feedMotor->getCurrentPosition();
    
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
    
    feedMotor->runForward(); // Continuous forward movement
    feedMotorMoving = true;
    feedStartTime = millis();
    Serial.println("Feed motor started - moving forward until distance sensor triggered");
    
    // Verify motor is actually running
    delay(10); // Brief delay to let motor start
    if (feedMotor->isRunning()) {
      // Check if position actually changed (motor is moving)
      delay(100); // Wait a bit longer to see movement
      int32_t newPosition = feedMotor->getCurrentPosition();
      if (newPosition == initialPosition) {
        Serial.println("FEED_TO_DISTANCE: WARNING - Motor position unchanged - motor may not be moving");
      } else {
        Serial.println("FEED_TO_DISTANCE: Motor movement confirmed - position changed from " + String(initialPosition) + " to " + String(newPosition));
      }
    } else {
      Serial.println("FEED_TO_DISTANCE: WARNING - Motor not running despite start command");
    }
  } else {
    Serial.println("FEED_TO_DISTANCE: ERROR - feedMotor pointer is NULL");
  }
}

void updateFeedToDistanceState() {
  // Update distance sensor
  feedDistanceSensor.update();
  
  // Update wood presence sensor
  woodPresenceSensor.update();
  
  //! ************************************************************************
  //! SAFETY CHECK: CYCLE SWITCH DEACTIVATION DETECTION
  //! ************************************************************************
  // Check if run cycle switch is turned off during operation - return to idle immediately
  if (!isRunCycleSwitchActive()) {
    Serial.println("FEED_TO_DISTANCE: Run cycle switch turned OFF - stopping operation");
    
    // Stop the feed motor immediately
    if (feedMotor && feedMotor->isRunning()) {
      feedMotor->forceStop();
      
      // Verify motor actually stopped
      delay(10);
      if (feedMotor->isRunning()) {
        Serial.println("FEED_TO_DISTANCE: WARNING - Motor still running after forceStop(), trying again");
        feedMotor->forceStop();
        delay(10);
        if (feedMotor->isRunning()) {
          Serial.println("FEED_TO_DISTANCE: ERROR - Motor still running after multiple stop attempts");
        } else {
          Serial.println("FEED_TO_DISTANCE: Motor stopped successfully on second attempt");
        }
      } else {
        Serial.println("FEED_TO_DISTANCE: Motor stopped successfully");
      }
      
      feedMotorMoving = false;
    }
    
    // Extend clamp to secure wood in current position
    extendClamp();
    
    // Return to idle state
    transitionToState(STATE_IDLE);
    return;
  }
  
  //! ************************************************************************
  //! SIMPLIFIED TIMEOUT DETECTION (2 SECOND LIMIT)
  //! ************************************************************************
  // Check for feed motor timeout (2 seconds) - if motor runs too long without sensor trigger, go to idle
  if (feedMotorMoving && !distanceSensorTriggered && !timeoutOccurred) {
    unsigned long elapsedTime = millis() - feedStartTime;
    
    // Single 2-second timeout for safety
    if (elapsedTime >= 2000) {
      timeoutOccurred = true;
      Serial.println("FEED_TO_DISTANCE: TIMEOUT - Feed motor ran for 2 seconds without triggering distance sensor");
    }
  }
  
  // Handle timeout when it occurs - stop motor and extend clamp immediately
  if (timeoutOccurred) {
    Serial.println("FEED_TO_DISTANCE: TIMEOUT - Feed motor ran for 2 seconds without triggering distance sensor");
    Serial.println("FEED_TO_DISTANCE: Timeout trigger details:");
    Serial.println("FEED_TO_DISTANCE:   - timeoutOccurred: " + String(timeoutOccurred));
    Serial.println("FEED_TO_DISTANCE:   - feedMotorMoving: " + String(feedMotorMoving));
    Serial.println("FEED_TO_DISTANCE:   - feedMotor exists: " + String(feedMotor ? "YES" : "NO"));
    Serial.println("FEED_TO_DISTANCE:   - feedMotor->isRunning(): " + String(feedMotor ? (feedMotor->isRunning() ? "TRUE" : "FALSE") : "N/A"));
    Serial.println("FEED_TO_DISTANCE:   - distanceSensorTriggered: " + String(distanceSensorTriggered));
    Serial.println("FEED_TO_DISTANCE:   - elapsed time: " + String(millis() - feedStartTime) + "ms");
    Serial.println("FEED_TO_DISTANCE: Returning to IDLE - cycle switch must be flipped OFF and ON to reset");
    
    // Stop the feed motor immediately
    if (feedMotor && feedMotor->isRunning()) {
      Serial.println("FEED_TO_DISTANCE: Stopping feed motor due to timeout");
      feedMotor->forceStop();
      
      // Verify motor actually stopped
      delay(10);
      if (feedMotor->isRunning()) {
        Serial.println("FEED_TO_DISTANCE: WARNING - Motor still running after forceStop(), trying again");
        feedMotor->forceStop();
        delay(10);
        if (feedMotor->isRunning()) {
          Serial.println("FEED_TO_DISTANCE: ERROR - Motor still running after multiple stop attempts");
        } else {
          Serial.println("FEED_TO_DISTANCE: Motor stopped successfully on second attempt");
        }
      } else {
        Serial.println("FEED_TO_DISTANCE: Motor stopped successfully");
      }
      
      feedMotorMoving = false;
    }
    
    // Extend clamp to secure wood in current position
    extendClamp();
    
    // Return to idle state
    transitionToState(STATE_IDLE);
    return;
  }
  

  
  // Check if distance sensor is triggered (active HIGH - HIGH when wood detected)
  if (feedDistanceSensor.read() == HIGH && !distanceSensorTriggered) {
    Serial.println("DISTANCE SENSOR TRIGGERED - Stopping feed motor");
    Serial.println("FEED_TO_DISTANCE: Distance sensor trigger details:");
    Serial.println("FEED_TO_DISTANCE:   - feedMotorMoving: " + String(feedMotorMoving));
    Serial.println("FEED_TO_DISTANCE:   - timeoutOccurred: " + String(timeoutOccurred));
    Serial.println("FEED_TO_DISTANCE:   - elapsed time: " + String(millis() - feedStartTime) + "ms");
    
    distanceSensorTriggered = true;
    
    // Stop the feed motor
    if (feedMotor) {
      Serial.println("FEED_TO_DISTANCE: Stopping feed motor due to distance sensor trigger");
      feedMotor->forceStop();
      
      // Verify motor actually stopped
      delay(10);
      if (feedMotor->isRunning()) {
        Serial.println("FEED_TO_DISTANCE: WARNING - Motor still running after forceStop(), trying again");
        feedMotor->forceStop();
        delay(10);
        if (feedMotor->isRunning()) {
          Serial.println("FEED_TO_DISTANCE: ERROR - Motor still running after multiple stop attempts");
        } else {
          Serial.println("FEED_TO_DISTANCE: Motor stopped successfully on second attempt");
        }
      } else {
        Serial.println("FEED_TO_DISTANCE: Motor stopped successfully");
      }
      
      feedMotorMoving = false;
    }
    
    // Extend clamp to secure wood
    extendClamp();
    
    // Start the delay timer
    delayStartTime = millis();
    Serial.println("Starting " + String(woodDistanceDelay) + "ms delay before cutting cycle");
    Serial.println("FEED_TO_DISTANCE: Motor stopped and clamp extended - waiting for delay");
  }
  
  // Check if delay is complete
  if (distanceSensorTriggered && !delayComplete && (millis() - delayStartTime >= woodDistanceDelay)) {
    delayComplete = true;
    Serial.println("Delay complete - checking conditions for cutting cycle");
    Serial.println("FEED_TO_DISTANCE: Delay completion details:");
    Serial.println("FEED_TO_DISTANCE:   - distanceSensorTriggered: " + String(distanceSensorTriggered));
    Serial.println("FEED_TO_DISTANCE:   - delayComplete: " + String(delayComplete));
    Serial.println("FEED_TO_DISTANCE:   - elapsed delay time: " + String(millis() - delayStartTime) + "ms");
    Serial.println("FEED_TO_DISTANCE:   - configured delay: " + String(woodDistanceDelay) + "ms");
    
    // Check both conditions before starting cutting cycle
    if (isRunCycleSwitchActive() && isWoodPresent()) {
      Serial.println("Conditions met - RUN CYCLE SWITCH ACTIVE & WOOD DETECTED - Starting cutting cycle");
      Serial.println("FEED_TO_DISTANCE: Transitioning directly to CUTTING state (includes complete cutting cycle)");
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
  // Note: Don't reset feedMotorMoving here as it might interfere with timeout logic
  Serial.println("FEED_TO_DISTANCE: Exiting state - cleaning up variables");
  Serial.println("FEED_TO_DISTANCE: Exit state - feedMotorMoving: " + String(feedMotorMoving) + ", timeoutOccurred: " + String(timeoutOccurred));
  
  distanceSensorTriggered = false;
  delayComplete = false;
  timeoutOccurred = false;
  
  // SIMPLIFIED: Remove unnecessary wood detection tracking
  // The system already knows wood is present when entering this state
  
  Serial.println("FEED_TO_DISTANCE: Exit state - variables reset");
}
