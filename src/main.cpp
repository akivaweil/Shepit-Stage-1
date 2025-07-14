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
//* *********************** SETUP FUNCTION ********************************
//* ************************************************************************
void setup() {
  //! ************************************************************************
  //! STEP 1: INITIALIZE OTA FUNCTIONALITY
  //! ************************************************************************
  setupOTA();

  //! ************************************************************************
  //! STEP 2: INITIALIZE BUTTON WITH PULLDOWN (ACTIVE HIGH)
  //! ************************************************************************
  button.attach(BUTTON_PIN, INPUT_PULLDOWN);
  button.interval(buttonDebounceTime);

  //! ************************************************************************
  //! STEP 3: INITIALIZE STEPPER MOTOR ENGINE
  //! ************************************************************************
  engine.init();
  
  // Create feed motor instance
  feedMotor = engine.stepperConnectToPin(FEED_MOTOR_STEP_PIN);
  if (feedMotor) {
    feedMotor->setDirectionPin(FEED_MOTOR_DIR_PIN);
    feedMotor->setSpeedInHz(feedMotorSpeed);
    feedMotor->setAcceleration(feedMotorAcceleration);
  }

  // Create cut motor instance
  cutMotor = engine.stepperConnectToPin(CUT_MOTOR_STEP_PIN);
  if (cutMotor) {
    cutMotor->setDirectionPin(CUT_MOTOR_DIR_PIN);
    cutMotor->setSpeedInHz(cutMotorSpeed);
    cutMotor->setAcceleration(cutMotorAcceleration);
  }

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
    // Start the sequence
    sequenceRunning = true;
    currentState = CUT_FORWARD;
    
    // Begin cut motor forward movement
    if (cutMotor) {
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
          currentState = CUT_BACKWARD;
          // Start cut motor backward movement
          cutMotor->move(-cutMotorSteps);
        }
        break;

      case CUT_BACKWARD:
        // Check if cut motor backward movement is complete
        if (cutMotor && !cutMotor->isRunning()) {
          currentState = FEED_FORWARD;
          // Start feed motor forward movement
          if (feedMotor) {
            feedMotor->move(feedMotorSteps);
          }
        }
        break;

      case FEED_FORWARD:
        // Check if feed motor movement is complete
        if (feedMotor && !feedMotor->isRunning()) {
          // Sequence complete, return to idle
          currentState = IDLE;
          sequenceRunning = false;
        }
        break;

      case IDLE:
        // Should not reach here during sequence
        break;
    }
  }

  //! ************************************************************************
  //! STEP 5: SMALL DELAY TO PREVENT WATCHDOG ISSUES
  //! ************************************************************************
  delay(10);
}
