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
  STATE_HOMING = 1,
  STATE_FEED_TO_DISTANCE = 2,
  STATE_CUTTING = 3,
  STATE_UNLOAD = 4
};

// System events for state machine
enum SystemEvent {
  EVENT_WOOD_DETECTED,
  EVENT_DISTANCE_SENSOR_TRIGGERED,
  EVENT_CUT_COMPLETE,
  EVENT_UNLOAD_REQUESTED,
  EVENT_RUN_CYCLE_ACTIVATED,
  EVENT_RUN_CYCLE_DEACTIVATED
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
//* *********************** STATE PATTERN *********************************
//* ************************************************************************
// State machine uses function-based implementation
// Each state has: enterState(), updateState(), exitState() functions

//* ************************************************************************
//* *********************** GLOBAL STATE VARIABLES ************************
//* ************************************************************************
extern SystemState currentSystemState;
extern SystemState previousSystemState;
extern unsigned long lastActivityTime;
extern bool motorsEnabled;

// Motor timeout and enable delay constants moved to Config.h

// Motor enable delay tracking
extern unsigned long motorEnableStartTime;
extern bool waitingForMotorEnable;

// Emergency stop tracking
extern unsigned long cycleStartTime;
extern bool emergencyStopRequested;
extern const unsigned long EMERGENCY_STOP_DELAY_MS;

// Startup safety: prevents automatic cycle start if run cycle switch is on at startup
extern bool startupSafetyResetRequired;

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
SystemState getCurrentState();

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

// Startup safety functions
void resetStartupSafety();
bool isStartupSafetyResetRequired();

// State machine reset functions
void resetAllStateMachineFlags();
void resetCuttingCycleFlags();
void resetFeedMotorFlags();
void resetMotorMovementFlags();

// Function to reset feed motor control variables (IDLE state specific)
void resetFeedMotorControlVariables(bool stopMotor = true);

// Forward clamp control functions
void extendForwardClamp();
void retractForwardClamp();
bool isForwardClampRetracted();

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
void initializeUnloadSwitch();
void initializeRedButton();
void initializeCutMotorHomeSwitch();
void initializeAllSensors();

// Sensor update functions
void updateFeedDistanceSensor();
void updateWoodPresenceSensor();
void updateRunCycleSwitch();
void updateUnloadSwitch();
void updateRedButton();
void updateCutMotorHomeSwitch();
void updateAllSensors();

// Sensor read functions
bool isFeedDistanceSensorTriggered();
bool isWoodPresenceSensorActive();
bool isRunCycleSwitchActive();
bool isRunCycleSwitchActiveCentralized();
bool isUnloadSwitchActive();
bool isRedButtonPressed();
bool isCutMotorHomeSwitchTriggered();

// Sensor reset functions
void resetFeedDistanceSensor();
void resetWoodPresenceSensor();
void resetRunCycleSwitch();
void resetUnloadSwitch();
void resetRedButton();
void resetCutMotorHomeSwitch();
void resetAllSensors();

// External references to sensor objects
extern Bounce2::Button feedDistanceSensor;
extern Bounce2::Button woodPresenceSensor;
extern Bounce2::Button runCycleSwitch;
extern Bounce2::Button unloadSwitch;
extern Bounce2::Button redButton;
// Cut motor home switch now uses direct digital reading - no Bounce2 object needed

// Continuous feed functions
void startContinuousFeed();
void stopContinuousFeed();

// Emergency stop function for feed operations
void emergencyStopFeedOperation();

// Unload mode state
extern bool unloadModeActive;

// Individual state functions
void enterIdleState();
void updateIdleState();
void exitIdleState();

// Function to reset feed motor control variables in IDLE state
void resetIdleFeedMotorControl(bool stopMotor = true);

void enterHomingState();
void updateHomingState();
void exitHomingState();

void enterFeedToDistanceState();
void updateFeedToDistanceState();
void exitFeedToDistanceState();

void enterCuttingState();
void updateCuttingState();
void exitCuttingState();

// Cutting cycle step functions
void updateActivateMotorsStep();
void updateRetractForwardClampStep();
void updateFeedForwardStep();
void updateExtendForwardClampStep();
void updateCutWoodStep();
void updateReturnCutMotorStep();
void updateCheckConditionsStep();

// Cutting cycle flag reset functions
void resetCutMotorStepFlags();
void resetReturnMotorStepFlags();

void enterUnloadState();
void updateUnloadState();
void exitUnloadState();

// Distance sensor functions
void initializeFeedDistanceSensor();
void updateFeedDistanceSensor();
bool isFeedDistanceSensorTriggered();
void resetFeedDistanceSensor();

#endif // STATEMACHINE_H 