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

// Wood sensor debouncer for IDLE state monitoring
static Bounce2::Button idleWoodSensor = Bounce2::Button();

// Run cycle switch debouncer for cutting cycle control
static Bounce2::Button runCycleSwitch = Bounce2::Button();

// Right switch debouncer for reload mode operation
static Bounce2::Button rightSwitch = Bounce2::Button();

// Red button debouncer for continuous feed operation
static Bounce2::Button redButton = Bounce2::Button();

// Distance sensor debouncer for feed motor stop control
static Bounce2::Button idleDistanceSensor = Bounce2::Button();

// Feed motor control state tracking
static bool feedMotorShouldRun = false;
static bool feedMotorWasRunning = false;
static unsigned long lastFeedMotorStateChange = 0;
static const unsigned long FEED_MOTOR_STATE_CHANGE_DELAY = 100; // 100ms minimum delay between state changes

// Feed motor timeout tracking for 2-second safety limit
static unsigned long feedMotorStartTime = 0;
static bool feedMotorTimeoutOccurred = false;
static bool feedMotorTimeoutLocked = false; // Prevents restart after timeout until conditions change

// Clamp state tracking to prevent rapid state changes
static bool clampShouldBeRetracted = false;
static bool clampWasRetracted = false;
static unsigned long lastClampStateChange = 0;
static const unsigned long CLAMP_STATE_CHANGE_DELAY = 200; // 200ms minimum delay between clamp state changes

void enterIdleState() {
  // Initialize wood sensor monitoring
  idleWoodSensor.attach(WOOD_PRESENT_SENSOR_PIN, INPUT);
  idleWoodSensor.interval(sensorDebounceTime); // Standard sensor debounce
  
  // Initialize run cycle switch monitoring
  runCycleSwitch.attach(RUN_CYCLE_SWITCH_PIN, INPUT);
  runCycleSwitch.interval(sensorDebounceTime); // Standard sensor debounce
  
  // Initialize right switch monitoring
  rightSwitch.attach(RIGHT_SWITCH_PIN, INPUT);
  rightSwitch.interval(sensorDebounceTime); // Standard sensor debounce
  
  // Initialize red button monitoring
  redButton.attach(RED_BUTTON_PIN, INPUT);
  redButton.interval(sensorDebounceTime); // Standard sensor debounce
  
  // Initialize distance sensor monitoring for feed motor stop control
  idleDistanceSensor.attach(WOOD_DISTANCE_SENSOR_PIN, INPUT);
  idleDistanceSensor.interval(distanceSensorDebounceTime); // Distance sensor debounce
  
  // Enable motors when entering idle state
  enableAllMotors();
  
  // Start 3-second motor timeout countdown for sleep mode
  resetMotorTimeout();
  
  // Reset manual mode flag
  manualMode = false;
  
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
}

void updateIdleState() {
  // Update all debouncers
  idleWoodSensor.update();
  runCycleSwitch.update();
  rightSwitch.update();
  redButton.update();
  idleDistanceSensor.update();
  
  //! ************************************************************************
  //! FEED MOTOR CONTROL LOGIC
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
  bool feedMotorShouldRun = runCycleActive && woodPresent && !inCuttingCycle && !feedMotorTimeoutLocked && (currentSystemState != STATE_RELOAD);
  
  // Unlock feed motor if conditions change (prevents infinite timeout loop)
  static bool previousRunCycleActive = false;
  static bool previousWoodPresent = false;
  
  // Unlock feed motor when:
  // 1. Conditions change (wood presence or run cycle switch)
  // 2. Run cycle switch is turned OFF (allows user to cancel and reset)
  // 3. Run cycle switch is turned ON after being OFF (allows user to restart)
  if (runCycleActive != previousRunCycleActive || woodPresent != previousWoodPresent) {
    if (feedMotorTimeoutLocked) {
      feedMotorTimeoutLocked = false;
      feedMotorTimeoutOccurred = false;
      Serial.println("Feed motor UNLOCKED - conditions changed, timeout reset");
    }
    previousRunCycleActive = runCycleActive;
    previousWoodPresent = woodPresent;
  }
  
  // Additional unlock logic: Allow user to reset timeout lock by cycling run cycle switch
  if (!runCycleActive && feedMotorTimeoutLocked) {
    // Run cycle switch is OFF and motor is locked - this allows user to cancel
    // The lock will be cleared when they turn the switch back ON
    static bool wasLockedWhenSwitchOff = false;
    if (!wasLockedWhenSwitchOff) {
      wasLockedWhenSwitchOff = true;
      Serial.println("Feed motor timeout lock active - turn run cycle switch OFF then ON to reset");
    }
  } else if (runCycleActive && feedMotorTimeoutLocked) {
    // Run cycle switch is ON and motor was locked - clear the lock to allow restart
    static bool wasLockedWhenSwitchOff = false;
    if (wasLockedWhenSwitchOff) {
      feedMotorTimeoutLocked = false;
      feedMotorTimeoutOccurred = false;
      wasLockedWhenSwitchOff = false;
      Serial.println("Feed motor UNLOCKED - run cycle switch cycled, timeout reset");
    }
  } else {
    // Reset the tracking variable when switch is ON and not locked
    static bool wasLockedWhenSwitchOff = false;
    wasLockedWhenSwitchOff = false;
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
          Serial.println("Clamp state: RETRACTED (feed motor running)");
        }
      } else {
        // Clamp should be extended
        if (isClampRetracted()) {
          extendClamp();
          Serial.println("Clamp state: EXTENDED (feed motor stopped)");
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
            Serial.println("Feed motor start: motors enabled from sleep mode");
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
          
          Serial.println("Feed motor started: run cycle ON + wood present");
        }
      } else {
        // Feed motor should NOT be running
        if (feedMotor && feedMotor->isRunning()) {
          // Stop the feed motor
          feedMotor->forceStop();
          
          // Reset timeout tracking when motor stops
          feedMotorTimeoutOccurred = false;
          
          if (inCuttingCycle) {
            Serial.println("Feed motor stopped: cutting cycle in progress (safety requirement)");
          } else if (!runCycleActive) {
            Serial.println("Feed motor stopped: run cycle switch turned OFF");
          } else if (!woodPresent) {
            Serial.println("Feed motor stopped: wood no longer present");
          }
        }
      }
      
      // Update the tracking variable
      feedMotorWasRunning = feedMotorShouldRun;
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
      Serial.println("FEED MOTOR STOPPED IMMEDIATELY: Run cycle switch turned OFF");
      feedMotor->forceStop();
      feedMotorWasRunning = false;
      feedMotorTimeoutOccurred = false;
      return; // Exit early to prevent further processing
    }
    
    // Check if wood is still present
    if (!woodPresent) {
      Serial.println("FEED MOTOR STOPPED IMMEDIATELY: Wood no longer present");
      feedMotor->forceStop();
      feedMotorWasRunning = false;
      feedMotorTimeoutOccurred = false;
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
      feedMotorTimeoutLocked = true; // Lock feed motor from restarting after timeout
      Serial.println("FEED MOTOR TIMEOUT - Motor running for 2+ seconds, stopping for safety");
      
      // Stop the feed motor immediately
      feedMotor->forceStop();
      
      // Extend clamp to secure wood
      extendClamp();
      
      // Update tracking variables
      feedMotorWasRunning = false;
      
      Serial.println("Feed motor stopped due to 2-second timeout - safety limit reached");
      Serial.println("Feed motor LOCKED - turn run cycle switch OFF then ON to reset and restart");
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
      
      Serial.println("Feed motor stopped: distance sensor triggered - starting cutting cycle");
    }
    
    // Start cutting cycle
    Serial.println("*** DISTANCE SENSOR TRIGGERED - Starting CUTTING CYCLE ***");
    Serial.println("Conditions met: Run cycle ON, Wood present, Distance sensor triggered");
    Serial.println("Transitioning to CUTTING state (includes positioning step)...");
    transitionToState(STATE_CUTTING);
    return; // Exit early since we're transitioning to cutting state
  }
  
  // Check for wood detection (active LOW - sensor reads 0 when wood detected)
  if (idleWoodSensor.fell()) {
    // Only start sequence if run cycle switch is active
    if (isRunCycleSwitchActive()) {
      Serial.println("WOOD DETECTED - RUN CYCLE SWITCH ACTIVE - Starting FEED TO DISTANCE sequence");
      transitionToState(STATE_FEED_TO_DISTANCE);
    } else {
      Serial.println("WOOD DETECTED - RUN CYCLE SWITCH NOT ACTIVE - Ignoring wood detection");
    }
    return;
  }
  
  // Check for run cycle switch activation (active HIGH - switch reads 1 when triggered)
  if (runCycleSwitch.rose()) {
    Serial.println("RUN CYCLE SWITCH ACTIVATED - Cutting cycle enabled");
    
    // If wood is present, feed motor will start automatically via the main control logic above
    if (isWoodPresent()) {
      Serial.println("WOOD PRESENT + RUN CYCLE SWITCH ACTIVE - Feed motor will start automatically");
    }
  }
  
  // Check for run cycle switch deactivation (switch released)
  if (runCycleSwitch.fell()) {
    Serial.println("RUN CYCLE SWITCH DEACTIVATED - Cutting cycle disabled");
    // Feed motor will stop automatically via the main control logic above
  }
  
  // Check for right switch activation (active HIGH - switch reads 1 when triggered)
  if (rightSwitch.rose()) {
    Serial.println("RIGHT SWITCH TRIGGERED - Starting RELOAD MODE");
    Serial.println("RELOAD MODE: Starting 5000-step reverse movement");
    transitionToState(STATE_RELOAD);
  }
  
  // Note: Right switch deactivation is now handled by the reload state itself
  // The reload state will automatically return to IDLE when the switch is released
  
  // Check for red button activation (active HIGH - button reads 1 when pressed)
  if (redButton.rose()) {
    Serial.println("RED BUTTON PRESSED - Starting continuous feed forward");
    startContinuousFeed();
  }
  
  // Check for red button deactivation (button released)
  if (redButton.fell()) {
    Serial.println("RED BUTTON RELEASED - Stopping continuous feed");
    stopContinuousFeed();
  }
  
  // Sleep mode functionality is handled by checkMotorTimeout() in main loop
}

void exitIdleState() {
  // Nothing specific needed when exiting idle state
  // Motor enable/disable is handled by the target state
} 