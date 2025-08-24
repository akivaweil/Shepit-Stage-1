#include <Arduino.h>
#include <FastAccelStepper.h>
#include <Bounce2.h>
#include "Config.h"
#include "Pins_Definitions.h"
#include "StateMachine.h"

//* ************************************************************************
//* ********************* DUAL MOTOR CONTROL MAIN *************************
//* ************************************************************************
// Dual stepper motor control with button-triggered sequence:
// 1. Cut motor moves forward 200 steps
// 2. Cut motor moves back 200 steps  
// 3. Feed motor moves forward 200 steps
// Uses FastAccelStepper library for smooth motor operation

// External OTA functions
extern void setupOTA();
extern void handleOTA();

//* ************************************************************************
//* *********************** MOTOR OBJECTS *********************************
//* ************************************************************************
FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *feedMotor = NULL;
FastAccelStepper *cutMotor = NULL;

//* ************************************************************************
//* *********************** BUTTON CONTROL ********************************
//* ************************************************************************
Bounce2::Button button = Bounce2::Button();

//* ************************************************************************
//* *********************** SERIAL COMMAND PROCESSING *********************
//* ************************************************************************
String inputString = "";
bool stringComplete = false;

//* ************************************************************************
//* *********************** SERIAL COMMAND HANDLER ************************
//* ************************************************************************
void processSerialCommand(String command) {
  command.trim();
  command.toLowerCase();
  
  Serial.println("Command received: " + command);
  

  
  // Motor enable/disable commands (motors now use sleep mode)
  if (command == "enablefeed") {
    enableAllMotors();
    Serial.println("Feed motor enabled (sleep mode after 3 seconds)");
  }
  else if (command == "disablefeed") {
    disableFeedMotor();
    Serial.println("Feed motor disabled");
  }
  else if (command == "enablecut") {
    enableAllMotors();
    Serial.println("Cut motor enabled (sleep mode after 3 seconds)");
  }
  else if (command == "disablecut") {
    disableCutMotor();
    Serial.println("Cut motor disabled");
  }
  else if (command == "disableall") {
    disableAllMotorsAfterDelay();
    Serial.println("All motors disabled");
  }
  
  // Pneumatic clamp commands
  else if (command == "clampextend") {
    extendForwardClamp();
    Serial.println("Forward clamp: EXTENDED");
  }
  else if (command == "clampretract") {
    retractForwardClamp();
    Serial.println("Forward clamp: RETRACTED");
  }
  
  // Feed motor movement commands
  else if (command == "feedforward") {
    if (feedMotor) {
      retractForwardClamp(); // Retract forward clamp before feed motor movement
      feedMotor->move(feedMotorSteps);
      Serial.println("Feed motor: moving forward");
    }
  }
  else if (command == "feedbackward") {
    if (feedMotor) {
      retractForwardClamp(); // Retract forward clamp before feed motor movement
      feedMotor->move(-feedMotorSteps);
      Serial.println("Feed motor: moving backward");
    }
  }
  else if (command == "feedstop") {
    if (feedMotor) {
      feedMotor->forceStop();
      Serial.println("Feed motor stopped");
    }
  }
  
  // Cut motor movement commands
  else if (command == "cutforward") {
    if (cutMotor) {
      cutMotor->move(cutMotorSteps);
      Serial.println("Cut motor moving forward");
    }
  }
  else if (command == "cutbackward") {
    if (cutMotor) {
      cutMotor->move(-cutMotorSteps);
      Serial.println("Cut motor moving backward");
    }
  }
  else if (command == "cutstop") {
    if (cutMotor) {
      cutMotor->forceStop();
      Serial.println("Cut motor stopped");
    }
  }
  
  // Custom step commands (format: feed500, cut-200, etc.)
  else if (command.startsWith("feed")) {
    String stepStr = command.substring(4);
    float steps = stepStr.toFloat();
    if (feedMotor && steps != 0) {
      retractForwardClamp(); // Retract forward clamp before feed motor movement
      feedMotor->move(steps);
      Serial.println("Feed motor moving " + String(steps) + " steps");
    }
  }
  else if (command.startsWith("cut")) {
    String stepStr = command.substring(3);
    float steps = stepStr.toFloat();
    if (cutMotor && steps != 0) {
      cutMotor->move(steps);
      Serial.println("Cut motor moving " + String(steps) + " steps");
    }
  }
  
  // Speed commands (format: feedspeed500, cutspeed100)
  else if (command.startsWith("feedspeed")) {
    String speedStr = command.substring(9);
    float speed = speedStr.toFloat();
    if (feedMotor && speed > 0) {
      feedMotor->setSpeedInHz(speed);
      Serial.println("Feed motor speed set to " + String(speed) + " Hz");
    }
  }
  else if (command.startsWith("cutspeed")) {
    String speedStr = command.substring(8);
    float speed = speedStr.toFloat();
    if (cutMotor && speed > 0) {
      cutMotor->setSpeedInHz(speed);
      Serial.println("Cut motor speed set to " + String(speed) + " Hz");
    }
  }
  
  // Status commands
  else if (command == "status") {
    Serial.println("=== SYSTEM STATUS ===");
    Serial.println("Current state: " + getCurrentStateName());
    Serial.println("Motors enabled: " + String(motorsEnabled));
  
    Serial.println("Run cycle switch: " + String(isRunCycleSwitchActive() ? "ACTIVE" : "INACTIVE"));
    Serial.println("Wood present: " + String(isWoodPresent() ? "YES" : "NO"));
    Serial.println("Wood at correct distance: " + String(isWoodAtCorrectDistance() ? "YES" : "NO"));
    Serial.println("Waiting for motor enable: " + String(waitingForMotorEnable));
    if (waitingForMotorEnable) {
      Serial.println("Motor enable delay remaining: " + String(motorEnableDelayMs - (millis() - motorEnableStartTime)) + "ms");
    }
    Serial.println("Feed motor running: " + String(feedMotor ? feedMotor->isRunning() : false));
    Serial.println("Cut motor running: " + String(cutMotor ? cutMotor->isRunning() : false));
    Serial.println("Feed motor position: " + String(feedMotor ? feedMotor->getCurrentPosition() : 0));
    Serial.println("Cut motor position: " + String(cutMotor ? cutMotor->getCurrentPosition() : 0));
    Serial.println("Forward clamp: " + String(isForwardClampRetracted() ? "RETRACTED" : "EXTENDED"));
    Serial.println("Reload mode active: " + String(currentSystemState == STATE_RELOAD ? "YES" : "NO"));
    Serial.println("Last activity: " + String(millis() - lastActivityTime) + "ms ago");
  }
  
  // Emergency stop
  else if (command == "stop" || command == "emergency") {
    handleEmergencyStop();
  }
  
  // Run sequence manually
  else if (command == "sequence") {
    if (isSystemIdle()) {
      // Check if run cycle switch is active and wood is present before starting
      if (isRunCycleSwitchActive() && isWoodPresent()) {
        Serial.println("Starting manual sequence - RUN CYCLE SWITCH ACTIVE & WOOD DETECTED");
        transitionToState(STATE_FEED_TO_DISTANCE);
      } else if (!isRunCycleSwitchActive()) {
        Serial.println("Cannot start sequence - RUN CYCLE SWITCH NOT ACTIVE");
      } else if (!isWoodPresent()) {
        Serial.println("Cannot start sequence - NO WOOD DETECTED");
      }
    } else {
      Serial.println("Sequence already running - current state: " + getCurrentStateName());
    }
  }
  
  // Reload mode commands
  else if (command == "reload") {
    if (isSystemIdle()) {
      // Check if right switch is active before starting reload mode
      if (digitalRead(RELOAD_SWITCH_PIN) == HIGH) {
        Serial.println("Starting reload mode - RIGHT SWITCH ACTIVE");
        transitionToState(STATE_RELOAD);
      } else {
        Serial.println("Cannot start reload mode - RIGHT SWITCH NOT ACTIVE");
      }
    } else {
      Serial.println("Cannot start reload mode - system busy, current state: " + getCurrentStateName());
    }
  }
  else if (command == "testreload") {
    Serial.println("=== RELOAD SWITCH TEST ===");
    Serial.println("Raw pin value: " + String(digitalRead(RELOAD_SWITCH_PIN)));
      Serial.println("Bounce2 state: " + String(reloadSwitch.read()));
  Serial.println("isReloadSwitchActive(): " + String(isReloadSwitchActive()));
    Serial.println("Current system state: " + getCurrentStateName());
    Serial.println("========================");
  }
  else if (command == "stopreload") {
    if (currentSystemState == STATE_RELOAD) {
      Serial.println("Stopping reload mode and returning to IDLE");
      transitionToState(STATE_IDLE);
    } else {
      Serial.println("Not in reload mode - current state: " + getCurrentStateName());
    }
  }
  
  // Return to idle from any state
  else if (command == "idle") {
    Serial.println("Returning to IDLE state");
    transitionToState(STATE_IDLE);
  }
  
  // Help command
  else if (command == "help") {
    Serial.println("=== AVAILABLE SERIAL COMMANDS ===");
    Serial.println("Motor Control:");
    Serial.println("  enablefeed/disablefeed - Enable/disable feed motor");
    Serial.println("  enablecut/disablecut - Enable/disable cut motor");
    Serial.println("  disableall - Disable all motors");
    Serial.println("Movement:");
    Serial.println("  feedforward/feedbackward - Move feed motor forward/backward");
    Serial.println("  cutforward/cutbackward - Move cut motor forward/backward");
    Serial.println("  feedstop/cutstop - Stop respective motor");
    Serial.println("  feed500, cut-200 - Move specific number of steps");
    Serial.println("  feedspeed500, cutspeed100 - Set motor speeds");
    Serial.println("Clamp Control:");
    Serial.println("  clampextend/clampretract - Control pneumatic clamp");
    Serial.println("System:");
    Serial.println("  status - Show system status");
    Serial.println("  sequence - Start cutting sequence manually");
    Serial.println("  reload - Start reload mode (feed motor moves 5000 steps reverse)");
    Serial.println("  testreload - Test reload switch status and values");
    Serial.println("  stopreload - Stop reload mode and return to IDLE");
    Serial.println("  stop/emergency - Emergency stop");
    Serial.println("  idle - Return to IDLE state");
    Serial.println("  help - Show this help");
  }
  
  // Unknown command
  else {
    Serial.println("Unknown command: " + command + " (type 'help' for available commands)");
  }
}

//* ************************************************************************
//* *********************** SETUP FUNCTION ********************************
//* ************************************************************************
void setup() {
  //! ************************************************************************
  //! STEP 1: INITIALIZE SERIAL COMMUNICATION
  //! ************************************************************************
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== DUAL MOTOR CONTROL SYSTEM STARTING ===");

  //! ************************************************************************
  //! STEP 2: INITIALIZE OTA FUNCTIONALITY
  //! ************************************************************************
  Serial.println("Initializing OTA...");
  setupOTA();
  Serial.println("OTA initialization complete");

  //! ************************************************************************
  //! STEP 3: INITIALIZE ENABLE PINS AND STATE MACHINE
  //! ************************************************************************
  Serial.println("Setting up motor enable pins...");
  pinMode(FEED_MOTOR_ENABLE_PIN, OUTPUT);
  pinMode(CUT_MOTOR_ENABLE_PIN, OUTPUT);
  
  // Enable motors on startup (will enter sleep mode after 3 seconds of idle)
  digitalWrite(FEED_MOTOR_ENABLE_PIN, LOW);  // Active low enable
  digitalWrite(CUT_MOTOR_ENABLE_PIN, LOW);   // Active low enable
  Serial.println("Motors enabled on startup - sleep mode after 3 seconds of idle");
  
  // Initialize state machine
  Serial.println("Initializing state machine...");
  initializeStateMachine();

  //! ************************************************************************
  //! STEP 4: INITIALIZE BUTTON WITH PULLDOWN (ACTIVE HIGH)
  //! ************************************************************************
  Serial.println("Setting up button control...");
  button.attach(BUTTON_PIN, INPUT_PULLDOWN);
  button.interval(buttonDebounceTime);
  Serial.println("Button setup complete");

  //! ************************************************************************
  //! STEP 5: INITIALIZE PNEUMATIC CLAMP RELAY
  //! ************************************************************************
  Serial.println("Setting up pneumatic clamp relay...");
  pinMode(CLAMP_RELAY_PIN, OUTPUT);
  extendForwardClamp(); // Start with forward clamp extended (ready position)
  Serial.println("Pneumatic clamp initialized - starting in extended position");

  //! ************************************************************************
  //! STEP 6: INITIALIZE STEPPER MOTOR ENGINE
  //! ************************************************************************
  Serial.println("Initializing stepper motor engine...");
  engine.init();
  
  // Create feed motor instance
  Serial.println("Setting up feed motor...");
  feedMotor = engine.stepperConnectToPin(FEED_MOTOR_STEP_PIN);
  if (feedMotor) {
    feedMotor->setDirectionPin(FEED_MOTOR_DIR_PIN);
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
    // Disable auto-enable since we handle it manually with delay
    feedMotor->setAutoEnable(false);
    // Set current position to 0 for reference
    feedMotor->setCurrentPosition(0);
    Serial.println("Feed motor configured successfully");
    Serial.println("Feed motor speed: " + String(feedMotorSpeed) + " Hz");
    Serial.println("Feed motor acceleration: " + String(feedMotorAcceleration) + " steps/s²");
  } else {
    Serial.println("ERROR: Failed to create feed motor instance");
  }

  // Create cut motor instance
  Serial.println("Setting up cut motor...");
  cutMotor = engine.stepperConnectToPin(CUT_MOTOR_STEP_PIN);
  if (cutMotor) {
    cutMotor->setDirectionPin(CUT_MOTOR_DIR_PIN);
    cutMotor->setSpeedInHz(cutMotorSpeed);
    cutMotor->setAcceleration(cutMotorAcceleration);
    // Disable auto-enable since we handle it manually with delay
    cutMotor->setAutoEnable(false);
    // Set current position to 0 for reference
    cutMotor->setCurrentPosition(0);
    Serial.println("Cut motor configured successfully");
    Serial.println("Cut motor speed: " + String(cutMotorSpeed) + " Hz");
    Serial.println("Cut motor acceleration: " + String(cutMotorAcceleration) + " steps/s²");
  } else {
    Serial.println("ERROR: Failed to create cut motor instance");
  }

  Serial.println("=== SYSTEM READY - WAITING FOR BUTTON PRESS ===");
  Serial.println("Type 'help' for available serial commands");
  delay(1000);
}

//* ************************************************************************
//* *********************** MAIN LOOP *************************************
//* ************************************************************************
void loop() {
  //! ************************************************************************
  //! STEP 1: HANDLE OTA UPDATES
  //! ************************************************************************
  handleOTA();

  //! ************************************************************************
  //! STEP 2: PROCESS SERIAL COMMANDS
  //! ************************************************************************
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      stringComplete = true;
    } else {
      inputString += inChar;
    }
  }
  
  if (stringComplete) {
    processSerialCommand(inputString);
    inputString = "";
    stringComplete = false;
  }

  //! ************************************************************************
  //! STEP 3: UPDATE BUTTON STATE
  //! ************************************************************************
  button.update();

  //! ************************************************************************
  //! STEP 4: CHECK FOR BUTTON PRESS TO START SEQUENCE OR EMERGENCY STOP
  //! ************************************************************************
  if (button.pressed()) {
    if (isSystemIdle()) {
      // Check if run cycle switch is active and wood is present before starting
      if (isRunCycleSwitchActive() && isWoodPresent()) {
        // Start new sequence - feed to distance then cutting cycle
        Serial.println("*** BUTTON PRESSED - RUN CYCLE SWITCH ACTIVE & WOOD DETECTED - STARTING FEED TO DISTANCE SEQUENCE ***");
        cycleStartTime = millis();
        emergencyStopRequested = false;
        transitionToState(STATE_FEED_TO_DISTANCE);
      } else if (!isRunCycleSwitchActive()) {
        Serial.println("*** BUTTON PRESSED - RUN CYCLE SWITCH NOT ACTIVE - Cannot start cutting cycle ***");
      } else if (!isWoodPresent()) {
        Serial.println("*** BUTTON PRESSED - NO WOOD DETECTED - Cannot start cutting cycle ***");
      }
    } 
    else if (isSystemBusy()) {
      // Check if enough time has passed to allow emergency stop (300ms)
      if (millis() - cycleStartTime >= EMERGENCY_STOP_DELAY_MS) {
        handleEmergencyStop();
      } else {
        Serial.println("Emergency stop blocked - wait " + String(EMERGENCY_STOP_DELAY_MS) + "ms after cycle start");
      }
    }
  }

  //! ************************************************************************
  //! STEP 5: UPDATE STATE MACHINE
  //! ************************************************************************
  updateStateMachine();

  //! ************************************************************************
  //! STEP 6: CHECK MOTOR TIMEOUT FOR SLEEP MODE
  //! ************************************************************************
  checkMotorTimeout();

  //! ************************************************************************
  //! STEP 7: SMALL DELAY TO PREVENT WATCHDOG ISSUES
  //! ************************************************************************
  delay(10);
}
