#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include <Arduino.h>
#include <FastAccelStepper.h>
#include <Bounce2.h>

//* ************************************************************************
//* *********************** STATE MACHINE DEFINITIONS *********************
//* ************************************************************************

// State enumeration
enum SystemState {
  STATE_IDLE = 0,
  STATE_FEED_TO_DISTANCE = 1,
  STATE_CUTTING = 2,
  STATE_MANUAL = 3,
  STATE_RELOAD = 4
};

// Cutting cycle step enumeration
enum CuttingStep {
  STEP_ACTIVATE_MOTORS,    // Step 1: Activate cut and feed motors
  STEP_RETRACT_CLAMP,      // Step 2: Retract the clamp
  STEP_FEED_FORWARD,       // Step 3: Feed wood forward until sensor triggers
  STEP_EXTEND_CLAMP,       // Step 4: Extend the clamp
  STEP_CUT_WOOD,           // Step 5: Cut wood by moving cut motor forward
  STEP_RETURN_CUT_MOTOR,   // Step 6: Return cut motor
  STEP_CHECK_CONDITIONS    // Step 7: Check conditions for next cycle
};

//* ************************************************************************
//* *********************** GLOBAL STATE VARIABLES ************************
//* ************************************************************************
extern SystemState currentSystemState;
extern SystemState previousSystemState;
extern unsigned long lastActivityTime;
extern bool motorsEnabled;
extern bool manualMode;
extern const unsigned long MOTOR_TIMEOUT_MS;
extern const unsigned long MOTOR_ENABLE_DELAY_MS;

// Motor enable delay tracking
extern unsigned long motorEnableStartTime;
extern bool waitingForMotorEnable;

// Emergency stop tracking
extern unsigned long cycleStartTime;
extern bool emergencyStopRequested;
extern const unsigned long EMERGENCY_STOP_DELAY_MS;

// Motor objects
extern FastAccelStepper *feedMotor;
extern FastAccelStepper *cutMotor;

//* ************************************************************************
//* *********************** FUNCTION DECLARATIONS *************************
//* ************************************************************************

// State machine core functions
void initializeStateMachine();
void updateStateMachine();
void transitionToState(SystemState newState);
String getCurrentStateName();

// System state functions
bool isSystemIdle();
bool isSystemBusy();
bool isInCuttingCycle();
void handleEmergencyStop();

// Motor control functions
void resetMotorTimeout();
void enableAllMotors();
void enableAllMotorsWithDelay();
bool isMotorEnableDelayComplete();
void disableAllMotorsAfterDelay();
void checkMotorTimeout();
void enableFeedMotor();
void disableFeedMotor();
void enableCutMotor();
void disableCutMotor();

// Feed motor timeout lock functions
void resetFeedMotorTimeoutLock();
bool isFeedMotorTimeoutLocked();
void setFeedMotorTimeoutLocked(bool locked);

// State machine reset functions
void resetAllStateMachineFlags();
void resetCuttingCycleFlags();
void resetFeedMotorFlags();
void resetMotorMovementFlags();

// Function to reset feed motor control variables (IDLE state specific)
void resetFeedMotorControlVariables();

// Pneumatic clamp control functions
void extendClamp();
void retractClamp();
bool isClampRetracted();

// Wood sensor functions
bool isWoodPresent();

// Wood distance sensor functions
bool isWoodAtCorrectDistance();

//* ************************************************************************
//* *********************** SENSOR SETUP & DEBOUNCING **********************
//* ************************************************************************

// Sensor initialization functions
void initializeFeedDistanceSensor();
void initializeWoodPresenceSensor();
void initializeRunCycleSwitch();
void initializeRightSwitch();
void initializeRedButton();
void initializeAllSensors();

// Sensor update functions
void updateFeedDistanceSensor();
void updateWoodPresenceSensor();
void updateRunCycleSwitch();
void updateRightSwitch();
void updateRedButton();
void updateAllSensors();

// Sensor read functions
bool isFeedDistanceSensorTriggered();
bool isWoodPresenceSensorActive();
bool isRunCycleSwitchActive();
bool isRunCycleSwitchActiveCentralized();
bool isRightSwitchActive();
bool isRedButtonPressed();

// Sensor reset functions
void resetFeedDistanceSensor();
void resetWoodPresenceSensor();
void resetRunCycleSwitch();
void resetRightSwitch();
void resetRedButton();
void resetAllSensors();

// External references to sensor objects
extern Bounce2::Button feedDistanceSensor;
extern Bounce2::Button woodPresenceSensor;
extern Bounce2::Button runCycleSwitch;
extern Bounce2::Button rightSwitch;
extern Bounce2::Button redButton;

// Continuous feed functions
void startContinuousFeed();
void stopContinuousFeed();

// Emergency stop function for feed operations
void emergencyStopFeedOperation();

// Reload mode state
extern bool reloadModeActive;

// Individual state functions
void enterIdleState();
void updateIdleState();
void exitIdleState();

// Function to reset feed motor control variables in IDLE state
void resetIdleFeedMotorControl();

void enterFeedToDistanceState();
void updateFeedToDistanceState();
void exitFeedToDistanceState();

void enterCuttingState();
void updateCuttingState();
void exitCuttingState();

// Cutting cycle step functions
void updateActivateMotorsStep();
void updateRetractClampStep();
void updateFeedForwardStep();
void updateExtendClampStep();
void updateCutWoodStep();
void updateReturnCutMotorStep();
void updateCheckConditionsStep();

// Cutting cycle flag reset functions
void resetCutMotorStepFlags();
void resetReturnMotorStepFlags();

void enterManualState();
void updateManualState();
void exitManualState();

void enterReloadState();
void updateReloadState();
void exitReloadState();

// Distance sensor functions
void initializeFeedDistanceSensor();
void updateFeedDistanceSensor();
bool isFeedDistanceSensorTriggered();
void resetFeedDistanceSensor();

#endif // STATEMACHINE_H 