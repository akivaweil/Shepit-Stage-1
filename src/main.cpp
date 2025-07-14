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
  //! STEP 2: UPDATE BUTTON STATE
  //! ************************************************************************
  button.update();

  //! ************************************************************************
  //! STEP 3: CHECK FOR BUTTON PRESS TO START SEQUENCE
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
  //! STEP 4: HANDLE SEQUENCE STATE MACHINE
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
  //! STEP 5: SMALL DELAY TO PREVENT WATCHDOG ISSUES
  //! ************************************************************************
  delay(10);
}
