#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"
#include <Bounce2.h>

// External state variable reference
extern SystemState currentSystemState;

//* ************************************************************************
//* ************************ IDLE STATE ***********************************
//* ************************************************************************
// The IDLE state is where the system waits for button press or manual commands
// Motors will automatically disable after 3 seconds of inactivity (sleep mode)
// 3-second countdown starts immediately when entering this state
// Also constantly monitors for wood detection to automatically start reloading
// Run cycle switch monitoring for cutting cycle control
// Feed motor control: runs continuously when run cycle switch ON + wood present
// Safety: Feed motor will NOT run during cutting cycles (prevents conflicts)
// Distance sensor trigger: starts cutting cycle when activated (if conditions still met)
// Note: Each cutting cycle includes its own positioning step to ensure wood is always in correct position

// Sensors are now handled centrally in Sensor_Setup.cpp

// Feed motor control state tracking - SIMPLIFIED
static bool feedMotorShouldRun = false;
static bool feedMotorWasRunning = false;
static unsigned long lastFeedMotorStateChange = 0;
static const unsigned long FEED_MOTOR_STATE_CHANGE_DELAY = 100; // 100ms minimum delay between state changes

// Feed motor timeout tracking for 2-second safety limit
static unsigned long feedMotorStartTime = 0;
static bool feedMotorTimeoutOccurred = false;
// Note: feedMotorTimeoutLocked is now a global variable in StateMachine_Functions.cpp

// Clamp state tracking to prevent rapid state changes
static bool clampShouldBeRetracted = false;
static bool clampWasRetracted = false;
static unsigned long lastClampStateChange = 0;
static const unsigned long CLAMP_STATE_CHANGE_DELAY = 200; // 200ms minimum delay between clamp state changes

void enterIdleState() {
  // Reset all state machine flags when entering IDLE state
  // This ensures a clean start and prevents any lingering flags from previous states
  resetAllStateMachineFlags();
  
  // Sensors are now initialized centrally in initializeStateMachine()
  
  // Enable motors when entering idle state
  enableAllMotors();
  
  // Start 3-second motor timeout countdown for sleep mode
  resetMotorTimeout();
  
  // Reset manual mode flag
  manualMode = false;
  
  // CRITICAL FIX: Reset feed motor control state - ensure clean start
  feedMotorShouldRun = false;
  feedMotorWasRunning = false;
  lastFeedMotorStateChange = 0;
  
  // Reset feed motor timeout tracking
  feedMotorTimeoutOccurred = false;
  
  // Reset clamp control state
  clampShouldBeRetracted = false;
  clampWasRetracted = false;
  lastClampStateChange = 0;
  
  // CRITICAL FIX: Ensure feed motor is stopped when entering IDLE state
  if (feedMotor && feedMotor->isRunning()) {
    Serial.println("IDLE: Stopping feed motor that was running from previous state");
    feedMotor->forceStop();
    delay(50); // Brief delay to ensure motor stops
  }
  
  // Check if this was an emergency stop before resetting the flag
  bool wasEmergencyStop = emergencyStopRequested;
  
  // Reset emergency stop flag
  emergencyStopRequested = false;
  
  if (wasEmergencyStop) {
    Serial.println("Emergency stop complete - system ready");
  } else {
    Serial.println("System ready");
  }
  
  Serial.println("Feed motor control: starts when run cycle switch ON + wood present, stops when distance sensor triggered");
  Serial.println("IDLE: Feed motor control variables reset - ready for clean operation");
}

void updateIdleState() {
  // Sensors are now updated centrally in updateStateMachine()
  
  // Debug: Verify sensor updates are working
  static unsigned long lastSensorDebug = 0;
  if (millis() - lastSensorDebug >= 2000) { // Log every 2 seconds
    Serial.println("IDLE: Sensor status - Right switch: " + String(isRightSwitchActive() ? "ACTIVE" : "INACTIVE") + 
                   ", Raw pin: " + String(digitalRead(RIGHT_SWITCH_PIN)) + 
                   ", Bounce2 state: " + String(rightSwitch.read()));
    lastSensorDebug = millis();
  }
  
  //! ************************************************************************
  //! FEED MOTOR CONTROL LOGIC - SIMPLIFIED AND FIXED
  //! ************************************************************************
  // Simplified logic: Feed motor runs continuously when conditions are met
  // Distance sensor triggers cutting cycle when activated
  // IMPORTANT: Feed motor will NOT run during cutting cycles for safety
  
  bool runCycleActive = isRunCycleSwitchActive();
  bool woodPresent = isWoodPresent();
  bool distanceSensorTriggered = isWoodAtCorrectDistance();
  bool inCuttingCycle = isInCuttingCycle();
  
  // Feed motor runs when run cycle switch is ON and wood is present
  // BUT NOT during cutting cycles (safety requirement)
  // AND NOT when locked due to timeout (prevents restart after timeout)
  // AND NOT when in reload state (prevents interference with reload operations)
  bool newFeedMotorShouldRun = runCycleActive && woodPresent && !inCuttingCycle && !isFeedMotorTimeoutLocked() && (currentSystemState != STATE_RELOAD);
  
  // CRITICAL FIX: Reset feed motor timeout lock when conditions change
  static bool previousRunCycleActive = false;
  static bool previousWoodPresent = false;
  
  // Unlock feed motor when conditions change
  if (runCycleActive != previousRunCycleActive || woodPresent != previousWoodPresent) {
    if (isFeedMotorTimeoutLocked()) {
      resetFeedMotorTimeoutLock();
      feedMotorTimeoutOccurred = false;
      Serial.println("Feed motor UNLOCKED - conditions changed, timeout reset");
    }
    previousRunCycleActive = runCycleActive;
    previousWoodPresent = woodPresent;
  }
  
  // CRITICAL FIX: Allow user to reset timeout lock by cycling run cycle switch
  if (!runCycleActive && isFeedMotorTimeoutLocked()) {
    // Run cycle switch is OFF and motor is locked - this allows user to cancel
    // The lock will be cleared when they turn the switch back ON
    static bool wasLockedWhenSwitchOff = false;
    if (!wasLockedWhenSwitchOff) {
      wasLockedWhenSwitchOff = true;
      Serial.println("Feed motor timeout lock active - turn run cycle switch OFF then ON to reset");
    }
  } else if (runCycleActive && isFeedMotorTimeoutLocked()) {
    // Run cycle switch is ON and motor was locked - clear the lock to allow restart
    static bool wasLockedWhenSwitchOff = false;
    if (wasLockedWhenSwitchOff) {
      resetFeedMotorTimeoutLock();
      feedMotorTimeoutOccurred = false;
      wasLockedWhenSwitchOff = false;
      Serial.println("Feed motor UNLOCKED - run cycle switch cycled, timeout reset");
    }
  } else {
    // Reset the tracking variable when switch is ON and not locked
    static bool wasLockedWhenSwitchOff = false;
    wasLockedWhenSwitchOff = false;
  }
  
  // CRITICAL FIX: Update feed motor should-run state and handle state changes
  if (newFeedMotorShouldRun != feedMotorShouldRun) {
    feedMotorShouldRun = newFeedMotorShouldRun;
    
    // Reset the was-running flag when conditions change to force state update
    feedMotorWasRunning = !feedMotorShouldRun;
    
    if (feedMotorShouldRun) {
      Serial.println("Feed motor conditions met: run cycle ON + wood present");
    } else {
      Serial.println("Feed motor conditions not met: stopping motor");
    }
  }
  
  // CRITICAL FIX: Additional check for cycle switch state changes
  // This ensures that when the cycle switch is toggled, the feed motor responds immediately
  static bool lastRunCycleState = false;
  if (runCycleActive != lastRunCycleState) {
    if (runCycleActive) {
      Serial.println("Cycle switch: ON - checking feed motor conditions");
      // CRITICAL FIX: Reset the was-running flag when cycle switch turns ON
      // This forces the motor to start if conditions are met
      feedMotorWasRunning = false;
      // CRITICAL FIX: Also reset the should-run state to force re-evaluation
      feedMotorShouldRun = false;
      Serial.println("Forcing feed motor state update due to cycle switch ON");
    } else {
      Serial.println("Cycle switch: OFF - stopping feed motor");
      // Ensure feed motor stops immediately
      if (feedMotor && feedMotor->isRunning()) {
        feedMotor->forceStop();
        feedMotorWasRunning = false;
        feedMotorTimeoutOccurred = false;
      }
      // CRITICAL FIX: Reset state tracking when cycle switch turns OFF
      // This ensures clean state when switch is turned back ON
      feedMotorShouldRun = false;
    }
    lastRunCycleState = runCycleActive;
  }
  
  // Determine if clamp should be retracted (retracted when feed motor is running)
  clampShouldBeRetracted = feedMotorShouldRun;
  
  // Control clamp state with protection against rapid changes
  if (clampShouldBeRetracted != clampWasRetracted) {
    // Check if enough time has passed since last clamp state change
    if (millis() - lastClampStateChange >= CLAMP_STATE_CHANGE_DELAY) {
      lastClampStateChange = millis();
      
      if (clampShouldBeRetracted) {
        // Clamp should be retracted
        if (!isClampRetracted()) {
          retractClamp();
          Serial.println("Clamp: RETRACTED (feed motor running)");
        }
      } else {
        // Clamp should be extended
        if (isClampRetracted()) {
          extendClamp();
          Serial.println("Clamp: EXTENDED (feed motor stopped)");
        }
      }
      
      // Update the tracking variable
      clampWasRetracted = clampShouldBeRetracted;
    }
  }
  
  // Control feed motor based on should-run state
  // Add protection against rapid state changes to prevent relay flickering
  if (feedMotorShouldRun != feedMotorWasRunning) {
    // Check if enough time has passed since last state change
    if (millis() - lastFeedMotorStateChange >= FEED_MOTOR_STATE_CHANGE_DELAY) {
      lastFeedMotorStateChange = millis();
      
      if (feedMotorShouldRun) {
        // Feed motor should be running
        if (feedMotor && !feedMotor->isRunning()) {
          // Ensure motors are enabled (wake from sleep mode if needed)
          if (!motorsEnabled) {
            enableAllMotors();
            Serial.println("Feed motor: motors enabled from sleep mode");
          }
          
          // Configure and start feed motor
          feedMotor->setSpeedInHz(feedMotorSpeed);
          feedMotor->setAcceleration(feedMotorAcceleration);
          feedMotor->runForward();
          
          // Start 2-second timeout tracking for feed motor safety
          // BUT NOT when in reload state (reload state has its own control logic)
          if (currentSystemState != STATE_RELOAD) {
            feedMotorStartTime = millis();
            feedMotorTimeoutOccurred = false;
          }
          
          // Reset motor timeout to keep motors enabled
          resetMotorTimeout();
          
          Serial.println("Feed motor: STARTED (run cycle ON + wood present)");
        }
      } else {
        // Feed motor should NOT be running
        if (feedMotor && feedMotor->isRunning()) {
          // Stop the feed motor
          feedMotor->forceStop();
          
          // Reset timeout tracking when motor stops
          feedMotorTimeoutOccurred = false;
          
          if (inCuttingCycle) {
            Serial.println("Feed motor: STOPPED (cutting cycle safety)");
          } else if (!runCycleActive) {
            Serial.println("Feed motor: STOPPED (cycle switch OFF)");
          } else if (!woodPresent) {
            Serial.println("Feed motor: STOPPED (wood not present)");
          }
        }
      }
      
      // Update the tracking variable
      feedMotorWasRunning = feedMotorShouldRun;
    }
  } else {
    // State variables are the same - no action needed
  }
  
  //! ************************************************************************
  //! IMMEDIATE FEED MOTOR STOP WHEN CONDITIONS CHANGE
  //! ************************************************************************
  // Continuously monitor conditions and stop feed motor immediately if they change
  // This ensures the motor stops as soon as the cycle switch is turned off or wood is removed
  if (feedMotor && feedMotor->isRunning() && !inCuttingCycle && (currentSystemState != STATE_RELOAD)) {
    // Check if run cycle switch is still active
    if (!runCycleActive) {
      Serial.println("Feed motor: EMERGENCY STOP (cycle switch OFF)");
      feedMotor->forceStop();
      feedMotorWasRunning = false;
      feedMotorTimeoutOccurred = false;
      // CRITICAL FIX: Update state tracking when emergency stopping
      feedMotorShouldRun = false;
      return; // Exit early to prevent further processing
    }
    
    // Check if wood is still present
    if (!woodPresent) {
      Serial.println("Feed motor: EMERGENCY STOP (wood not present)");
      feedMotor->forceStop();
      feedMotorWasRunning = false;
      feedMotorTimeoutOccurred = false;
      // CRITICAL FIX: Update state tracking when emergency stopping
      feedMotorShouldRun = false;
      return; // Exit early to prevent further processing
    }
  }
  
  //! ************************************************************************
  //! FEED MOTOR TIMEOUT CHECK (2-SECOND SAFETY LIMIT)
  //! ************************************************************************
  // Check if feed motor has been running for more than 2 seconds without distance sensor trigger
  // BUT NOT when in reload state (reload state has its own control logic)
  if (feedMotor && feedMotor->isRunning() && !feedMotorTimeoutOccurred && (currentSystemState != STATE_RELOAD)) {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - feedMotorStartTime;
    
    if (elapsedTime >= 2000) {
      feedMotorTimeoutOccurred = true;
      setFeedMotorTimeoutLocked(true); // Lock feed motor from restarting after timeout
      Serial.println("Feed motor: SAFETY TIMEOUT (2+ seconds) - stopping motor");
      
      // Stop the feed motor immediately
      feedMotor->forceStop();
      
      // Extend clamp to secure wood
      extendClamp();
      
      // Update tracking variables
      feedMotorWasRunning = false;
      
      Serial.println("Feed motor: LOCKED - cycle switch OFF then ON to reset");
    }
  }
  
  //! ************************************************************************
  //! DISTANCE SENSOR TRIGGER FOR CUTTING CYCLE
  //! ************************************************************************
  // Check if distance sensor is triggered to start cutting cycle
  if (distanceSensorTriggered && runCycleActive && woodPresent) {
    // Stop feed motor if it's running
    if (feedMotor && feedMotor->isRunning()) {
      feedMotor->forceStop();
      
      // Reset timeout tracking when motor stops due to distance sensor
      feedMotorTimeoutOccurred = false;
      
      Serial.println("Feed motor: STOPPED (distance sensor triggered)");
    }
    
    // Start cutting cycle
    Serial.println("*** STARTING CUTTING CYCLE ***");
    Serial.println("Conditions: Run cycle ON, Wood present, Distance sensor triggered");
    transitionToState(STATE_CUTTING);
    return; // Exit early since we're transitioning to cutting state
  }
  
  // Check for wood detection (active LOW - sensor reads 0 when wood detected)
  // Note: Edge detection is now handled by centralized sensor functions
  // The main control logic above handles wood detection automatically
  
  // Check for run cycle switch activation (active HIGH - switch reads 1 when triggered)
  // Note: Edge detection is now handled by centralized sensor functions
  // The main control logic above handles switch state changes automatically
  
  // Check for right switch activation (active HIGH - switch reads 1 when triggered)
  // Note: Edge detection is now handled by centralized sensor functions
  // The main control logic above handles switch state changes automatically
  
  // Check for red button activation (active HIGH - button reads 1 when pressed)
  // Note: Edge detection is now handled by centralized sensor functions
  // The main control logic above handles button state changes automatically
  
  //! ************************************************************************
  //! RELOAD SWITCH MONITORING - AUTOMATIC RELOAD STATE TRANSITION
  //! ************************************************************************
  // Check if reload switch (right switch) is activated to automatically start reload mode
  bool reloadSwitchActive = isRightSwitchActive();
  static bool reloadSwitchWasActive = false;
  
  // Debug logging for reload switch monitoring
  static unsigned long lastReloadSwitchDebug = 0;
  if (millis() - lastReloadSwitchDebug >= 1000) { // Log every second
    Serial.println("IDLE: Monitoring reload switch - State: " + String(reloadSwitchActive ? "ACTIVE" : "INACTIVE") + 
                   " (Raw pin value: " + String(digitalRead(RIGHT_SWITCH_PIN)) + ")");
    lastReloadSwitchDebug = millis();
  }
  
  // Detect rising edge of reload switch (switch turned ON)
  if (reloadSwitchActive && !reloadSwitchWasActive) {
    Serial.println("*** RELOAD SWITCH ACTIVATED - Starting reload mode automatically ***");
    
    // Stop feed motor if it's running before transitioning to reload
    if (feedMotor && feedMotor->isRunning()) {
      feedMotor->forceStop();
      Serial.println("Feed motor: STOPPED (transitioning to reload mode)");
    }
    
    // Extend clamp to secure wood before reload operation
    if (!isClampRetracted()) {
      extendClamp();
      Serial.println("Clamp: EXTENDED (securing wood for reload operation)");
    }
    
    // Transition to reload state
    transitionToState(STATE_RELOAD);
    return; // Exit early since we're transitioning to reload state
  }
  
  // Update tracking variable
  reloadSwitchWasActive = reloadSwitchActive;
  
  // Sleep mode functionality is handled by checkMotorTimeout() in main loop
}

void exitIdleState() {
  // Nothing specific needed when exiting idle state
  // Motor enable/disable is handled by the target state
}

// Function to reset feed motor control variables (can be called externally)
void resetIdleFeedMotorControl() {
  Serial.println("IDLE: Resetting feed motor control variables");
  
  // Reset feed motor control state
  feedMotorShouldRun = false;
  feedMotorWasRunning = false;
  lastFeedMotorStateChange = 0;
  
  // Reset feed motor timeout tracking
  feedMotorTimeoutOccurred = false;
  
  // Reset clamp control state
  clampShouldBeRetracted = false;
  clampWasRetracted = false;
  lastClampStateChange = 0;
  
  // Ensure feed motor is stopped
  if (feedMotor && feedMotor->isRunning()) {
    Serial.println("IDLE: Stopping feed motor during reset");
    feedMotor->forceStop();
  }
  
  Serial.println("IDLE: Feed motor control variables reset complete");
} 