#include <Arduino.h>
#include <FastAccelStepper.h>

//* ************************************************************************
//* *********************** MOTOR CONTROL MAIN ****************************
//* ************************************************************************
// Simple stepper motor control that moves 200 steps continuously
// Uses FastAccelStepper library for smooth motor operation

// External OTA functions
extern void setupOTA();
extern void handleOTA();

//* ************************************************************************
//* *********************** PIN DEFINITIONS *******************************
//* ************************************************************************
#define STEP_PIN 5
#define DIR_PIN 6

//* ************************************************************************
//* *********************** MOTOR CONFIGURATION ****************************
//* ************************************************************************
FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper = NULL;

// Motor parameters
const float stepsPerMove = 1000.0;
const float motorSpeed = 500.0;      // Steps per second
const float motorAcceleration = 2000.0; // Steps per second^2

//* ************************************************************************
//* *********************** SETUP FUNCTION ********************************
//* ************************************************************************
void setup() {
  //! ************************************************************************
  //! STEP 1: INITIALIZE OTA FUNCTIONALITY
  //! ************************************************************************
  setupOTA();

  //! ************************************************************************
  //! STEP 2: INITIALIZE STEPPER MOTOR ENGINE
  //! ************************************************************************
  engine.init();
  
  // Create stepper instance
  stepper = engine.stepperConnectToPin(STEP_PIN);
  if (stepper) {
    stepper->setDirectionPin(DIR_PIN);
    stepper->setSpeedInHz(motorSpeed);
    stepper->setAcceleration(motorAcceleration);
  }

  //! ************************************************************************
  //! STEP 3: INITIAL MOTOR MOVEMENT
  //! ************************************************************************
  // Start the first 200-step movement
  if (stepper) {
    stepper->move(stepsPerMove);
  }
  delay(2000);
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
  //! STEP 2: CHECK IF MOTOR MOVEMENT IS COMPLETE
  //! ************************************************************************
  if (stepper && !stepper->isRunning()) {
    // Motor has completed its 200 steps, start the next movement
    stepper->move(stepsPerMove);

  }

  //! ************************************************************************
  //! STEP 3: SMALL DELAY TO PREVENT WATCHDOG ISSUES
  //! ************************************************************************
  delay(10);
}
