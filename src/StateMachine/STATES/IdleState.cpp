#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ IDLE STATE CLASS IMPLEMENTATION ***************
//* ************************************************************************

class IdleState : public State {
private:
  // Private variables - only this class can access them
  bool feedMotorShouldRun;
  bool feedMotorWasRunning;
  unsigned long lastStateChange;
  const unsigned long STATE_CHANGE_DELAY = 100;
  
  // Feed motor timeout tracking
  unsigned long feedMotorStartTime;
  bool feedMotorTimeoutOccurred;
  
  // Clamp state tracking
  bool clampShouldBeRetracted;
  bool clampWasRetracted;
  unsigned long lastClampStateChange;
  const unsigned long CLAMP_STATE_CHANGE_DELAY = 100;
  
  // Reload switch monitoring
  bool reloadSwitchWasActive;
  
  // State transition requests
  bool transitionRequested;
  SystemState requestedState;
  
public:
  // Constructor - called when creating IdleState object
  IdleState() {
    // Initialize private variables
    feedMotorShouldRun = false;
    feedMotorWasRunning = false;
    lastStateChange = 0;
    feedMotorStartTime = 0;
    feedMotorTimeoutOccurred = false;
    clampShouldBeRetracted = false;
    clampWasRetracted = false;
    lastClampStateChange = 0;
    reloadSwitchWasActive = false;
    transitionRequested = false;
    requestedState = STATE_IDLE;
  }
  
  // Implement the required virtual functions
  void enter() override {
    Serial.println("IDLE: Entering idle state");
    
    // Reset all state machine flags when entering IDLE state
    resetAllStateMachineFlags();
    
    // Reset feed motor control state - ensure clean start
    feedMotorShouldRun = false;
    feedMotorWasRunning = false;
    lastStateChange = 0;
    
    // Reset feed motor timeout tracking
    feedMotorTimeoutOccurred = false;
    
    // Reset feed motor timeout lock when entering IDLE state
    resetFeedMotorTimeoutLock();
    
    // Reset clamp control state
    clampShouldBeRetracted = false;
    clampWasRetracted = false;
    lastClampStateChange = 0;
    
    // Ensure feed motor is stopped when entering IDLE state
    if (feedMotor && feedMotor->isRunning()) {
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
    
    Serial.println("Feed motor control ready");
    Serial.println("IDLE: Ready");
    
    // Enable motors when entering idle state
    enableAllMotors();
    
    // Start 3-second motor timeout countdown for sleep mode
    resetMotorTimeout();
  }
  
  void update() override {
    // This runs every loop iteration while in IDLE state
    
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
    bool newFeedMotorShouldRun = runCycleActive && woodPresent && !inCuttingCycle && 
                                 !isFeedMotorTimeoutLocked() && (currentSystemState != STATE_RELOAD);
    
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
    static bool wasLockedWhenSwitchOff = false;
    
    if (!runCycleActive && isFeedMotorTimeoutLocked()) {
      // Run cycle switch is OFF and motor is locked - this allows user to cancel
      // The lock will be cleared when they turn the switch back ON
      if (!wasLockedWhenSwitchOff) {
        wasLockedWhenSwitchOff = true;
        Serial.println("Feed motor: LOCKED");
      }
    } else if (runCycleActive && isFeedMotorTimeoutLocked()) {
      // Run cycle switch is ON and motor was locked - clear the lock to allow restart
      if (wasLockedWhenSwitchOff) {
        resetFeedMotorTimeoutLock();
        feedMotorTimeoutOccurred = false;
        wasLockedWhenSwitchOff = false;
        Serial.println("Feed motor UNLOCKED - run cycle switch cycled, timeout reset");
      }
    } else {
      // Reset the tracking variable when switch is ON and not locked
      wasLockedWhenSwitchOff = false;
    }
    
    // CRITICAL FIX: Update feed motor should-run state and handle state changes
    if (newFeedMotorShouldRun != feedMotorShouldRun) {
      feedMotorShouldRun = newFeedMotorShouldRun;
      
      // Reset the was-running flag when conditions change to force state update
      feedMotorWasRunning = !feedMotorShouldRun;
      
      if (feedMotorShouldRun) {
        Serial.println("Feed motor: ON");
      } else {
        Serial.println("Feed motor: OFF");
      }
    }
    
    // CRITICAL FIX: Force motor state update when conditions change
    // This ensures the motor responds immediately to condition changes
    static bool lastConditions = false;
    bool currentConditions = runCycleActive && woodPresent && !inCuttingCycle && 
                            !isFeedMotorTimeoutLocked() && (currentSystemState != STATE_RELOAD);
    
    if (currentConditions != lastConditions) {
      // Conditions changed - force motor state update
      feedMotorWasRunning = !currentConditions;
      lastConditions = currentConditions;
      Serial.println("Feed motor: Conditions changed, forcing state update");
    }
    
    // CRITICAL FIX: Additional check for cycle switch state changes
    // This ensures that when the cycle switch is toggled, the feed motor responds immediately
    static bool lastRunCycleState = false;
    if (runCycleActive != lastRunCycleState) {
      if (runCycleActive) {
        Serial.println("Cycle switch: ON");
        // CRITICAL FIX: Clear timeout lock immediately when cycle switch turns ON
        if (isFeedMotorTimeoutLocked()) {
          resetFeedMotorTimeoutLock();
          feedMotorTimeoutOccurred = false;
          wasLockedWhenSwitchOff = false;
          Serial.println("Feed motor UNLOCKED - cycle switch turned ON, timeout reset");
        }
        // CRITICAL FIX: Reset the was-running flag when cycle switch turns ON
        // This forces the motor to start if conditions are met
        feedMotorWasRunning = false;
      } else {
        Serial.println("Cycle switch: OFF");
        // Ensure feed motor stops immediately
        if (feedMotor && feedMotor->isRunning()) {
          feedMotor->forceStop();
          feedMotorWasRunning = false;
          feedMotorTimeoutOccurred = false;
        }
        // CRITICAL FIX: Reset state tracking when cycle switch turns OFF
        // This ensures clean state when switch is turned back ON
        feedMotorShouldRun = false;
        // CRITICAL FIX: Reset the was-running flag to ensure motor can restart
        feedMotorWasRunning = false;
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
          if (!isForwardClampRetracted()) {
            retractForwardClamp();
            Serial.println("Forward clamp: RETRACTED");
          }
        } else {
          // Forward clamp should be extended
          if (isForwardClampRetracted()) {
            extendForwardClamp();
            Serial.println("Forward clamp: EXTENDED");
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
      if (millis() - lastStateChange >= STATE_CHANGE_DELAY) {
        lastStateChange = millis();
        
        if (feedMotorShouldRun) {
          // Feed motor should be running
          if (feedMotor && !feedMotor->isRunning()) {
            // Ensure motors are enabled (wake from sleep mode if needed)
            if (!motorsEnabled) {
              enableAllMotors();
              Serial.println("Feed motor: Motors enabled");
            }
            
            // CRITICAL FIX: Brief delay to ensure motor is ready after previous stop
            if (feedMotorWasRunning) {
              delay(50); // 50ms delay to ensure motor is ready for new commands
            }
            
            // Configure and start feed motor
            feedMotor->setSpeedInHz(feedMotorSpeed);
            feedMotor->setAcceleration(feedMotorAcceleration);
            feedMotor->runForward();
            
            // CRITICAL FIX: Verify motor actually started
            delay(10); // Brief delay to let motor start
            if (!feedMotor->isRunning()) {
              // Motor failed to start - reset state to allow retry
              Serial.println("Feed motor: FAILED TO START - resetting state for retry");
              feedMotorWasRunning = false; // Reset to allow retry
              return; // Exit early to prevent further processing
            }
            
            // Start 2-second timeout tracking for feed motor safety
            // BUT NOT when in reload state (reload state has its own control logic)
            if (currentSystemState != STATE_RELOAD) {
              feedMotorStartTime = millis();
              feedMotorTimeoutOccurred = false;
            }
            
            // Reset motor timeout to keep motors enabled
            resetMotorTimeout();
            
            Serial.println("Feed motor: STARTED");
          } else {
            Serial.println("Feed motor: Already running or motor object not available");
          }
        } else {
          // Feed motor should NOT be running
          if (feedMotor && feedMotor->isRunning()) {
            // Stop the feed motor
            feedMotor->forceStop();
            
            // Reset timeout tracking when motor stops
            feedMotorTimeoutOccurred = false;
            
            if (inCuttingCycle) {
              Serial.println("Feed motor: STOPPED");
            } else if (!runCycleActive) {
              Serial.println("Feed motor: STOPPED");
            } else if (!woodPresent) {
              Serial.println("Feed motor: STOPPED");
            }
          }
        }
        
        // Update the tracking variable
        feedMotorWasRunning = feedMotorShouldRun;
      }
    } else {
      // State variables are the same - no action needed
      // Debug: Log when motor should be running but isn't
      if (feedMotorShouldRun && feedMotor && !feedMotor->isRunning()) {
        Serial.println("Feed motor: Should be running but isn't - checking conditions");
        Serial.println("  runCycleActive: " + String(runCycleActive));
        Serial.println("  woodPresent: " + String(woodPresent));
        Serial.println("  inCuttingCycle: " + String(inCuttingCycle));
        Serial.println("  isFeedMotorTimeoutLocked: " + String(isFeedMotorTimeoutLocked()));
        Serial.println("  currentSystemState: " + String(currentSystemState));
        Serial.println("  feedMotorShouldRun: " + String(feedMotorShouldRun));
        Serial.println("  feedMotorWasRunning: " + String(feedMotorWasRunning));
        
        // CRITICAL FIX: If motor should be running but isn't, reset state to allow retry
        // This prevents the system from getting stuck in a loop
        static unsigned long lastMotorRetryTime = 0;
        if (millis() - lastMotorRetryTime >= 1000) { // Retry every 1 second
          lastMotorRetryTime = millis();
          Serial.println("Feed motor: RESETTING STATE TO ALLOW RETRY");
          feedMotorWasRunning = false; // Reset to force motor start attempt
        }
      }
    }
    
    //! ************************************************************************
    //! IMMEDIATE FEED MOTOR STOP WHEN CONDITIONS CHANGE
    //! ************************************************************************
    // Continuously monitor conditions and stop feed motor immediately if they change
    // This ensures the motor stops as soon as the cycle switch is turned off or wood is removed
    if (feedMotor && feedMotor->isRunning() && !inCuttingCycle && (currentSystemState != STATE_RELOAD)) {
      // Check if run cycle switch is still active
      if (!runCycleActive) {
        Serial.println("Feed motor: EMERGENCY STOP");
        feedMotor->forceStop();
        feedMotorWasRunning = false;
        feedMotorTimeoutOccurred = false;
        // CRITICAL FIX: Update state tracking when emergency stopping
        feedMotorShouldRun = false;
        // CRITICAL FIX: Reset all feed motor state to ensure clean restart
        lastStateChange = 0;
        return; // Exit early to prevent further processing
      }
      
      // Check if wood is still present
      if (!woodPresent) {
        Serial.println("Feed motor: EMERGENCY STOP");
        feedMotor->forceStop();
        feedMotorWasRunning = false;
        feedMotorTimeoutOccurred = false;
        // CRITICAL FIX: Update state tracking when emergency stopping
        feedMotorShouldRun = false;
        // CRITICAL FIX: Reset all feed motor state to ensure clean restart
        lastStateChange = 0;
        return; // Exit early to prevent further processing
      }
    }
    
    //! ************************************************************************
    //! FEED MOTOR TIMEOUT CHECK (CONFIGURABLE SAFETY LIMIT)
    //! ************************************************************************
    // Check if feed motor has been running for more than the configured timeout without distance sensor trigger
    // BUT NOT when in reload state (reload state has its own control logic)
    if (feedMotor && feedMotor->isRunning() && !feedMotorTimeoutOccurred && (currentSystemState != STATE_RELOAD)) {
      unsigned long currentTime = millis();
      unsigned long elapsedTime = currentTime - feedMotorStartTime;
      
      if (elapsedTime >= feedMotorTimeout) {
        feedMotorTimeoutOccurred = true;
        setFeedMotorTimeoutLocked(true); // Lock feed motor from restarting after timeout
        Serial.println("Feed motor: SAFETY TIMEOUT (" + String(feedMotorTimeout/1000) + "s)");
        
        // Stop the feed motor immediately
        feedMotor->forceStop();
        
        // Extend forward clamp to secure wood
        extendForwardClamp();
        
        // Update tracking variables
        feedMotorWasRunning = false;
        
        Serial.println("Feed motor: LOCKED");
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
        
        Serial.println("Feed motor: STOPPED");
      }
      
      // Start cutting cycle
      Serial.println("*** STARTING CUTTING CYCLE ***");
      // Request state transition to cutting state
      transitionRequested = true;
      requestedState = STATE_CUTTING;
      return; // Exit early since we're transitioning to cutting state
    }
    
    //! ************************************************************************
    //! RELOAD SWITCH MONITORING - AUTOMATIC RELOAD STATE TRANSITION
    //! ************************************************************************
    // Check if reload switch (right switch) is activated to automatically start reload mode
    bool reloadSwitchActive = isReloadSwitchActive();
    
    // Detect rising edge of reload switch (switch turned ON)
    if (reloadSwitchActive && !reloadSwitchWasActive) {
      Serial.println("*** RELOAD SWITCH ACTIVATED - Starting reload mode automatically ***");
      
      // Stop feed motor if it's running before transitioning to reload
      if (feedMotor && feedMotor->isRunning()) {
        feedMotor->forceStop();
        Serial.println("Feed motor: STOPPED");
      }
      
      // Extend forward clamp to secure wood before reload operation
      if (!isForwardClampRetracted()) {
        extendForwardClamp();
        Serial.println("Forward clamp: EXTENDED");
      }
      
      // Request state transition to reload state
      transitionRequested = true;
      requestedState = STATE_RELOAD;
      return; // Exit early since we're transitioning to reload state
    }
    
    // Update tracking variable
    reloadSwitchWasActive = reloadSwitchActive;
    
    // Sleep mode functionality is handled by checkMotorTimeout() in main loop
  }
  
  void exit() override {
    Serial.println("IDLE: Exiting idle state");
    
    // Nothing specific needed when exiting idle state
    // Motor enable/disable is handled by the target state
  }
  
  // Implement required identification functions
  SystemState getStateId() const override { 
    return STATE_IDLE; 
  }
  
  const char* getStateName() const override { 
    return "IDLE"; 
  }
  
  // Override transition validation
  bool canTransitionTo(SystemState targetState) const override {
    // Only allow valid transitions from IDLE
    return (targetState == STATE_FEED_TO_DISTANCE || 
            targetState == STATE_CUTTING || 
            targetState == STATE_RELOAD);
  }
  
  // Override timeout handling
  unsigned long getStateTimeout() const override { 
    return 0; // No timeout for IDLE 
  }
  
  void handleTimeout() override {
    // No timeout handling needed for IDLE
  }
  
  // Event handling
  void handleEvent(SystemEvent event) override {
    switch(event) {
      case EVENT_WOOD_DETECTED:
        // Handle wood detection
        break;
      case EVENT_DISTANCE_SENSOR_TRIGGERED:
        // Start cutting cycle
        break;
      case EVENT_RELOAD_REQUESTED:
        // Transition to reload
        break;
      default:
        // Ignore other events
        break;
    }
  }
  
  // Transition request methods
  bool hasTransitionRequest() const { return transitionRequested; }
  SystemState getRequestedState() const { return requestedState; }
  void clearTransitionRequest() { 
    transitionRequested = false; 
    requestedState = STATE_IDLE; 
  }
};
