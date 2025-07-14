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
  
  // Transition to manual mode for motor commands
  if (!manualMode && (command.startsWith("feed") || command.startsWith("cut") || 
                      command == "enablefeed" || command == "disablefeed" ||
                      command == "enablecut" || command == "disablecut")) {
    transitionToState(STATE_MANUAL);
  }
  
  // Motor enable/disable commands (legacy support - motors are permanently enabled)
  if (command == "enablefeed") {
    Serial.println("Feed motor is permanently ENABLED");
  }
  else if (command == "disablefeed") {
    Serial.println("Motors are permanently enabled - cannot disable");
  }
  else if (command == "enablecut") {
    Serial.println("Cut motor is permanently ENABLED");
  }
  else if (command == "disablecut") {
    Serial.println("Motors are permanently enabled - cannot disable");
  }
  else if (command == "disableall") {
    Serial.println("Motors are permanently enabled - cannot disable");
  }
  
  // Feed motor movement commands
  else if (command == "feedforward") {
    if (feedMotor) {
      feedMotor->move(feedMotorSteps);
      Serial.println("Feed motor moving forward");
    }
  }
  else if (command == "feedbackward") {
    if (feedMotor) {
      feedMotor->move(-feedMotorSteps);
      Serial.println("Feed motor moving backward");
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
    Serial.println("Manual mode: " + String(manualMode));
    Serial.println("Waiting for motor enable: " + String(waitingForMotorEnable));
    if (waitingForMotorEnable) {
      Serial.println("Motor enable delay remaining: " + String(MOTOR_ENABLE_DELAY_MS - (millis() - motorEnableStartTime)) + "ms");
    }
    Serial.println("Feed motor running: " + String(feedMotor ? feedMotor->isRunning() : false));
    Serial.println("Cut motor running: " + String(cutMotor ? cutMotor->isRunning() : false));
    Serial.println("Feed motor position: " + String(feedMotor ? feedMotor->getCurrentPosition() : 0));
    Serial.println("Cut motor position: " + String(cutMotor ? cutMotor->getCurrentPosition() : 0));
    Serial.println("Last activity: " + String(millis() - lastActivityTime) + "ms ago");
  }
  
  // Emergency stop
  else if (command == "stop" || command == "emergency") {
    handleEmergencyStop();
  }
  
  // Run sequence manually
  else if (command == "sequence") {
    if (isSystemIdle()) {
      Serial.println("Starting manual sequence...");
      transitionToState(STATE_CUTTING);
    } else {
      Serial.println("Sequence already running - current state: " + getCurrentStateName());
    }
  }
  
  // Return to idle from manual mode
  else if (command == "idle") {
    transitionToState(STATE_IDLE);
    Serial.println("Returning to idle state");
  }
  
  // Help command
  else if (command == "help") {
    Serial.println("=== AVAILABLE COMMANDS ===");
    Serial.println("Motor Control:");
    Serial.println("  enablefeed, disablefeed, enablecut, disablecut, disableall");
    Serial.println("Basic Movement:");
    Serial.println("  feedforward, feedbackward, cutforward, cutbackward");
    Serial.println("  feedstop, cutstop");
    Serial.println("Custom Steps:");
    Serial.println("  feed[number] (e.g., feed500, feed-200)");
    Serial.println("  cut[number] (e.g., cut100, cut-50)");
    Serial.println("Speed Control:");
    Serial.println("  feedspeed[number] (e.g., feedspeed500)");
    Serial.println("  cutspeed[number] (e.g., cutspeed100)");
    Serial.println("System:");
    Serial.println("  status, stop, emergency, sequence, help");
  }
  
  else {
    Serial.println("Unknown command: " + command);
    Serial.println("Type 'help' for available commands");
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
  
  // Enable motors permanently on startup
  digitalWrite(FEED_MOTOR_ENABLE_PIN, LOW);  // Active low enable
  digitalWrite(CUT_MOTOR_ENABLE_PIN, LOW);   // Active low enable
  Serial.println("Motors ENABLED permanently on startup");
  
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
  //! STEP 5: INITIALIZE STEPPER MOTOR ENGINE
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
    Serial.println("Button pressed - Current state: " + getCurrentStateName());
    
    if (isSystemIdle()) {
      // Start new cutting cycle
      Serial.println("*** BUTTON PRESSED - STARTING SEQUENCE ***");
      cycleStartTime = millis();
      emergencyStopRequested = false;
      transitionToState(STATE_CUTTING);
    } 
    else if (isSystemBusy()) {
      // Emergency stop requires a second button press after cycle starts
      unsigned long timeSinceStart = millis() - cycleStartTime;
      Serial.println("System busy - Time since cycle start: " + String(timeSinceStart) + "ms");
      
      if (timeSinceStart >= EMERGENCY_STOP_DELAY_MS) {
        Serial.println("*** EMERGENCY STOP TRIGGERED - Button pressed again during operation ***");
        handleEmergencyStop();
      } else {
        Serial.println("Emergency stop blocked - wait " + String(EMERGENCY_STOP_DELAY_MS - timeSinceStart) + "ms more after cycle start");
      }
    }
  }

  //! ************************************************************************
  //! STEP 5: UPDATE STATE MACHINE
  //! ************************************************************************
  updateStateMachine();

  //! ************************************************************************
  //! STEP 6: CHECK MOTOR IDLE TIMEOUT
  //! ************************************************************************
  checkMotorTimeout();

  //! ************************************************************************
  //! STEP 7: SMALL DELAY TO PREVENT WATCHDOG ISSUES
  //! ************************************************************************
  delay(10);
}
