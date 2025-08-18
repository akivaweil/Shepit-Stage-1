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
static bool timeoutOccurred = false; // New timeout flag for more reliable detection
static unsigned long lastTimeoutCheck = 0; // Track last timeout check time

//! ************************************************************************
//! WOOD DETECTION LOGIC VARIABLES
//! ************************************************************************
// Variables to track wood detection state (simplified - no step counting)
static bool woodDetectedAtStart = false;
static bool woodLostDuringFeed = false;

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
  lastTimeoutCheck = 0;
  
  //! ************************************************************************
  //! INITIALIZE WOOD DETECTION LOGIC VARIABLES
  //! ************************************************************************
  woodDetectedAtStart = isWoodPresent();
  woodLostDuringFeed = false;
  
  Serial.println("FEED_TO_DISTANCE: Wood detection initialized - woodDetectedAtStart: " + String(woodDetectedAtStart ? "YES" : "NO"));
  
  // Check if run cycle switch is still active before proceeding
  if (!isRunCycleSwitchActive()) {
    Serial.println("FEED_TO_DISTANCE: RUN CYCLE SWITCH NOT ACTIVE - Returning to IDLE immediately");
    transitionToState(STATE_IDLE);
    return;
  }
  
  // Reset feed motor timeout lock when entering this state
  // This ensures the lock is cleared regardless of which state we came from
  if (isFeedMotorTimeoutLocked()) {
    Serial.println("FEED_TO_DISTANCE: Resetting feed motor timeout lock from previous state");
    resetFeedMotorTimeoutLock();
  }
  
  // Ensure motors are enabled
  enableAllMotors();
  
  // Verify motors are actually enabled
  if (!motorsEnabled) {
    Serial.println("FEED_TO_DISTANCE: WARNING - Motors may not be enabled, attempting to enable again");
    enableAllMotors();
    delay(50); // Give motors time to enable
  }
  
  Serial.println("FEED_TO_DISTANCE: Motor enable state - motorsEnabled: " + String(motorsEnabled));
  
  // Check actual motor enable pin state
  Serial.println("FEED_TO_DISTANCE: Feed motor enable pin state: " + String(digitalRead(FEED_MOTOR_ENABLE_PIN)));
  Serial.println("FEED_TO_DISTANCE: Cut motor enable pin state: " + String(digitalRead(CUT_MOTOR_ENABLE_PIN)));
  
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
    
    Serial.println("FEED_TO_DISTANCE: Configuring feed motor - Speed: " + String(feedMotorSpeed) + "Hz, Accel: " + String(feedMotorAcceleration));
    
    // Get initial position for movement verification
    int32_t initialPosition = feedMotor->getCurrentPosition();
    Serial.println("FEED_TO_DISTANCE: Initial motor position: " + String(initialPosition));
    
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
    
    // Check motor state before starting
    Serial.println("FEED_TO_DISTANCE: Motor state before start - isRunning(): " + String(feedMotor->isRunning()));
    
    feedMotor->runForward(); // Continuous forward movement
    feedMotorMoving = true;
    feedStartTime = millis();
    lastTimeoutCheck = millis(); // Initialize timeout check time
    Serial.println("Feed motor started - moving forward until distance sensor triggered");
    Serial.println("FEED_TO_DISTANCE: Timeout initialized at " + String(feedStartTime) + "ms, will timeout at " + String(feedStartTime + 2000) + "ms");
    
    // Verify motor is actually running
    delay(10); // Brief delay to let motor start
    if (feedMotor->isRunning()) {
      Serial.println("FEED_TO_DISTANCE: Motor verification - feedMotor->isRunning() = TRUE");
      
      // Check if position actually changed (motor is moving)
      delay(100); // Wait a bit longer to see movement
      int32_t newPosition = feedMotor->getCurrentPosition();
      Serial.println("FEED_TO_DISTANCE: Motor position after 100ms: " + String(newPosition));
      if (newPosition == initialPosition) {
        Serial.println("FEED_TO_DISTANCE: WARNING - Motor position unchanged - motor may not be moving despite isRunning() = TRUE");
      } else {
        Serial.println("FEED_TO_DISTANCE: Motor movement confirmed - position changed from " + String(initialPosition) + " to " + String(newPosition));
      }
    } else {
      Serial.println("FEED_TO_DISTANCE: WARNING - Motor verification - feedMotor->isRunning() = FALSE");
      Serial.println("FEED_TO_DISTANCE: Motor may not be running despite start command");
      Serial.println("FEED_TO_DISTANCE: Check motor enable state and connections");
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
  //! WOOD DETECTION LOGIC - CHECK FOR WOOD LOSS DURING FEEDING
  //! ************************************************************************
  // Monitor wood presence during feeding and handle wood loss logic
  if (feedMotorMoving && !distanceSensorTriggered && !timeoutOccurred) {
    bool currentWoodPresent = isWoodPresent();
    
    // Debug wood presence status with debounced sensor values
    static unsigned long lastWoodCheckTime = 0;
    if (millis() - lastWoodCheckTime >= 500) { // Log every 500ms
      int rawWoodSensorValue = digitalRead(WOOD_PRESENT_SENSOR_PIN);
      int debouncedWoodSensorValue = woodPresenceSensor.read();
      Serial.println("FEED_TO_DISTANCE: Wood presence check - currentWoodPresent: " + String(currentWoodPresent ? "YES" : "NO") + 
                    ", raw sensor pin: " + String(rawWoodSensorValue) + 
                    ", debounced sensor: " + String(debouncedWoodSensorValue) + 
                    ", woodDetectedAtStart: " + String(woodDetectedAtStart ? "YES" : "NO") + 
                    ", woodLostDuringFeed: " + String(woodLostDuringFeed ? "YES" : "NO"));
      lastWoodCheckTime = millis();
    }
    
    // Check if wood was detected at start but is now lost
    if (woodDetectedAtStart && !currentWoodPresent && !woodLostDuringFeed) {
      woodLostDuringFeed = true;
      Serial.println("FEED_TO_DISTANCE: WOOD LOST DURING FEEDING - Stopping feed motor immediately for safety");
      
      // Stop the feed motor immediately when wood is lost
      if (feedMotor && feedMotor->isRunning()) {
        Serial.println("FEED_TO_DISTANCE: Stopping feed motor due to wood loss");
        feedMotor->forceStop();
        feedMotorMoving = false;
        
        // Extend clamp to secure wood in current position
        extendClamp();
        
        // Return to idle state since wood was lost
        Serial.println("FEED_TO_DISTANCE: Returning to IDLE - wood lost during feeding");
        transitionToState(STATE_IDLE);
        return;
      }
    }
    
    // Continuous check: if wood is lost and we're not in wood detection mode, start it
    if (woodDetectedAtStart && !currentWoodPresent && !woodLostDuringFeed) {
      Serial.println("FEED_TO_DISTANCE: WARNING - Wood lost but wood detection not activated, forcing activation");
      woodLostDuringFeed = true;
      
      // Stop the feed motor immediately when wood is lost
      if (feedMotor && feedMotor->isRunning()) {
        Serial.println("FEED_TO_DISTANCE: Stopping feed motor due to wood loss (forced activation)");
        feedMotor->forceStop();
        feedMotorMoving = false;
        
        // Extend clamp to secure wood in current position
        extendClamp();
        
        // Return to idle state since wood was lost
        Serial.println("FEED_TO_DISTANCE: Returning to IDLE - wood lost during feeding (forced activation)");
        transitionToState(STATE_IDLE);
        return;
      }
    }
    
    // Note: Removed the 1500-step continuation logic since we now stop immediately when wood is lost
    // This makes the system more responsive and safer
  }
  
  //! ************************************************************************
  //! SAFETY CHECK: CYCLE SWITCH DEACTIVATION DETECTION
  //! ************************************************************************
  // Check if run cycle switch is turned off during operation - return to idle immediately
  if (!isRunCycleSwitchActive()) {
    Serial.println("FEED_TO_DISTANCE: RUN CYCLE SWITCH TURNED OFF - Stopping operation and returning to IDLE");
    
    // Stop the feed motor immediately
    if (feedMotor && feedMotor->isRunning()) {
      Serial.println("FEED_TO_DISTANCE: Stopping feed motor due to run cycle switch OFF");
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
  //! TIMEOUT DETECTION AND HANDLING (2 SECOND LIMIT)
  //! ************************************************************************
  // Check for feed motor timeout (2 seconds) - if motor runs too long without sensor trigger, go to idle
  // Note: Since we now stop immediately when wood is lost, we use a standard 2-second timeout
  if (feedMotorMoving && !distanceSensorTriggered && !timeoutOccurred) {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - feedStartTime;
    
    // Use standard 2-second timeout since wood loss now stops motor immediately
    unsigned long timeoutDuration = 2000; // 2 seconds standard timeout
    
    // Check for timeout every 100ms to ensure reliable detection
    if (currentTime - lastTimeoutCheck >= 100) {
      lastTimeoutCheck = currentTime;
      
      // Debug timeout progress
      if (elapsedTime % 500 == 0) { // Log every 500ms
        Serial.println("FEED_TO_DISTANCE: Timeout progress - " + String(elapsedTime) + "ms elapsed, " + String(timeoutDuration - elapsedTime) + "ms remaining");
      }
      
      if (elapsedTime >= timeoutDuration) {
        timeoutOccurred = true;
        Serial.println("FEED_TO_DISTANCE: TIMEOUT DETECTED at " + String(elapsedTime) + "ms");
        Serial.println("FEED_TO_DISTANCE: Timeout variables - feedMotorMoving: " + String(feedMotorMoving) + ", distanceSensorTriggered: " + String(distanceSensorTriggered) + ", timeoutOccurred: " + String(timeoutOccurred));
      }
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
  
  //! ************************************************************************
  //! DEBUG LOGGING FOR TIMEOUT MONITORING
  //! ************************************************************************
  // Debug logging for timeout troubleshooting - improved timing logic
  if (feedMotorMoving && !distanceSensorTriggered && !timeoutOccurred) {
    unsigned long elapsedTime = millis() - feedStartTime;
    static unsigned long lastDebugTime = 0;
    
    // Use standard 2-second timeout for debug logging
    unsigned long timeoutDuration = 2000;
    
    // Log every 500ms for better debugging
    if (elapsedTime - lastDebugTime >= 500) {
      Serial.println("FEED_TO_DISTANCE: Motor running for " + String(elapsedTime) + "ms, timeout at " + String(timeoutDuration) + "ms");
      Serial.println("FEED_TO_DISTANCE: Debug - feedMotorMoving: " + String(feedMotorMoving) + ", distanceSensorTriggered: " + String(distanceSensorTriggered));
      if (feedMotor) {
        Serial.println("FEED_TO_DISTANCE: Debug - feedMotor->isRunning(): " + String(feedMotor->isRunning()));
        // Additional motor state verification
        if (!feedMotor->isRunning()) {
          Serial.println("FEED_TO_DISTANCE: WARNING - Motor stopped unexpectedly at " + String(elapsedTime) + "ms");
          feedMotorMoving = false; // Update flag to match actual state
        }
      }
      lastDebugTime = elapsedTime;
    }
    
    // Additional debug info when approaching timeout
    if (elapsedTime >= (timeoutDuration - 200) && elapsedTime < timeoutDuration) {
      Serial.println("FEED_TO_DISTANCE: WARNING - Approaching timeout in " + String(timeoutDuration - elapsedTime) + "ms");
    }
    
    // Force timeout check every 100ms when approaching timeout
    if (elapsedTime >= (timeoutDuration - 100)) {
      Serial.println("FEED_TO_DISTANCE: CRITICAL - At " + String(elapsedTime) + "ms, forcing timeout check");
    }
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
  lastTimeoutCheck = 0;
  
  //! ************************************************************************
  //! RESET WOOD DETECTION LOGIC VARIABLES
  //! ************************************************************************
  woodDetectedAtStart = false;
  woodLostDuringFeed = false;
  
  Serial.println("FEED_TO_DISTANCE: Exit state - variables reset");
}
