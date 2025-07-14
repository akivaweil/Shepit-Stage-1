#include <Arduino.h>
#include <FastAccelStepper.h>
#include <Bounce2.h>
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ********************* DUAL MOTOR CONTROL MAIN *************************
//* ************************************************************************
// Dual stepper motor control with button-triggered sequence:
// 1. Cut motor moves forward 500 steps
// 2. Cut motor moves back 500 steps
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
//* *********************** SEQUENCE CONTROL ******************************
//* ************************************************************************
enum SequenceState {
  IDLE,
  CUT_FORWARD,
  CUT_BACKWARD,
  FEED_FORWARD
};

SequenceState currentState = IDLE;
bool sequenceRunning = false;

//* ************************************************************************
//* *********************** SERIAL COMMAND PROCESSING *********************
//* ************************************************************************
String inputString = "";
bool stringComplete = false;

//* ************************************************************************
//* *********************** MOTOR ENABLE FUNCTIONS ************************
//* ************************************************************************
void enableFeedMotor() {
  digitalWrite(FEED_MOTOR_ENABLE_PIN, LOW);  // Active low enable
  Serial.println("Feed motor ENABLED");
}

void disableFeedMotor() {
  digitalWrite(FEED_MOTOR_ENABLE_PIN, HIGH); // Active low enable
  Serial.println("Feed motor DISABLED");
}

void enableCutMotor() {
  digitalWrite(CUT_MOTOR_ENABLE_PIN, LOW);   // Active low enable
  Serial.println("Cut motor ENABLED");
}

void disableCutMotor() {
  digitalWrite(CUT_MOTOR_ENABLE_PIN, HIGH);  // Active low enable
  Serial.println("Cut motor DISABLED");
}

void disableAllMotors() {
  disableFeedMotor();
  disableCutMotor();
  Serial.println("All motors DISABLED");
}

//* ************************************************************************
//* *********************** SERIAL COMMAND HANDLER ************************
//* ************************************************************************
void processSerialCommand(String command) {
  command.trim();
  command.toLowerCase();
  
  Serial.println("Command received: " + command);
  
  // Motor enable/disable commands
  if (command == "enablefeed") {
    enableFeedMotor();
  }
  else if (command == "disablefeed") {
    disableFeedMotor();
  }
  else if (command == "enablecut") {
    enableCutMotor();
  }
  else if (command == "disablecut") {
    disableCutMotor();
  }
  else if (command == "disableall") {
    disableAllMotors();
  }
  
  // Feed motor movement commands
  else if (command == "feedforward") {
    if (feedMotor) {
      enableFeedMotor();
      feedMotor->move(feedMotorSteps);
      Serial.println("Feed motor moving forward");
    }
  }
  else if (command == "feedbackward") {
    if (feedMotor) {
      enableFeedMotor();
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
      enableCutMotor();
      cutMotor->move(cutMotorSteps);
      Serial.println("Cut motor moving forward");
    }
  }
  else if (command == "cutbackward") {
    if (cutMotor) {
      enableCutMotor();
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
      enableFeedMotor();
      feedMotor->move(steps);
      Serial.println("Feed motor moving " + String(steps) + " steps");
    }
  }
  else if (command.startsWith("cut")) {
    String stepStr = command.substring(3);
    float steps = stepStr.toFloat();
    if (cutMotor && steps != 0) {
      enableCutMotor();
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
    Serial.println("Feed motor running: " + String(feedMotor ? feedMotor->isRunning() : false));
    Serial.println("Cut motor running: " + String(cutMotor ? cutMotor->isRunning() : false));
    Serial.println("Feed motor position: " + String(feedMotor ? feedMotor->getCurrentPosition() : 0));
    Serial.println("Cut motor position: " + String(cutMotor ? cutMotor->getCurrentPosition() : 0));
    Serial.println("Sequence running: " + String(sequenceRunning));
    Serial.println("Current state: " + String(currentState));
  }
  
  // Emergency stop
  else if (command == "stop" || command == "emergency") {
    if (feedMotor) feedMotor->forceStop();
    if (cutMotor) cutMotor->forceStop();
    disableAllMotors();
    sequenceRunning = false;
    currentState = IDLE;
    Serial.println("EMERGENCY STOP - All motors stopped and disabled");
  }
  
  // Run sequence manually
  else if (command == "sequence") {
    if (!sequenceRunning) {
      Serial.println("Starting manual sequence...");
      sequenceRunning = true;
      currentState = CUT_FORWARD;
      enableCutMotor();
      if (cutMotor) {
        cutMotor->move(cutMotorSteps);
      }
    } else {
      Serial.println("Sequence already running");
    }
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
  //! STEP 3: INITIALIZE ENABLE PINS
  //! ************************************************************************
  Serial.println("Setting up motor enable pins...");
  pinMode(FEED_MOTOR_ENABLE_PIN, OUTPUT);
  pinMode(CUT_MOTOR_ENABLE_PIN, OUTPUT);
  
  // Disable all motors initially
  disableAllMotors();

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
    Serial.println("Feed motor configured successfully");
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
    Serial.println("Cut motor configured successfully");
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
  //! STEP 4: CHECK FOR BUTTON PRESS TO START SEQUENCE
  //! ************************************************************************
  if (button.pressed() && !sequenceRunning) {
    Serial.println("*** BUTTON PRESSED - STARTING SEQUENCE ***");
    
    // Start the sequence
    sequenceRunning = true;
    currentState = CUT_FORWARD;
    
    // Enable cut motor and begin forward movement
    enableCutMotor();
    if (cutMotor) {
      Serial.println("Starting cut motor forward movement (500 steps)");
      cutMotor->move(cutMotorSteps);
    }
  }

  //! ************************************************************************
  //! STEP 5: HANDLE SEQUENCE STATE MACHINE
  //! ************************************************************************
  if (sequenceRunning) {
    switch (currentState) {
      case CUT_FORWARD:
        // Check if cut motor forward movement is complete
        if (cutMotor && !cutMotor->isRunning()) {
          Serial.println("Cut motor forward movement COMPLETE");
          currentState = CUT_BACKWARD;
          
          // Start cut motor backward movement
          Serial.println("Starting cut motor backward movement (500 steps)");
          cutMotor->move(-cutMotorSteps);
        }
        break;

      case CUT_BACKWARD:
        // Check if cut motor backward movement is complete
        if (cutMotor && !cutMotor->isRunning()) {
          Serial.println("Cut motor backward movement COMPLETE");
          
          // Disable cut motor and enable feed motor
          disableCutMotor();
          enableFeedMotor();
          
          currentState = FEED_FORWARD;
          
          // Start feed motor forward movement
          if (feedMotor) {
            Serial.println("Starting feed motor forward movement (200 steps)");
            feedMotor->move(feedMotorSteps);
          }
        }
        break;

      case FEED_FORWARD:
        // Check if feed motor movement is complete
        if (feedMotor && !feedMotor->isRunning()) {
          Serial.println("Feed motor forward movement COMPLETE");
          
          // Disable feed motor
          disableFeedMotor();
          
          // Sequence complete, return to idle
          currentState = IDLE;
          sequenceRunning = false;
          
          Serial.println("*** SEQUENCE COMPLETE - READY FOR NEXT BUTTON PRESS ***");
        }
        break;

      case IDLE:
        // Should not reach here during sequence
        Serial.println("ERROR: Reached IDLE state during sequence");
        break;
    }
  }

  //! ************************************************************************
  //! STEP 6: SMALL DELAY TO PREVENT WATCHDOG ISSUES
  //! ************************************************************************
  delay(10);
}
