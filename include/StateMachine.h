#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include <Arduino.h>
#include <FastAccelStepper.h>

//* ************************************************************************
//* *********************** STATE MACHINE DEFINITIONS *********************
//* ************************************************************************

// State enumeration
enum SystemState {
  STATE_IDLE = 0,
  STATE_CUTTING = 1,
  STATE_RETURNING = 2,
  STATE_FEEDING = 3,
  STATE_MANUAL = 4
};

// Cutting phase enumeration (no longer used - kept for compatibility)
enum CuttingPhase {
  CUT_FORWARD_PHASE,
  CUT_BACKWARD_PHASE
};

// Feeding phase enumeration
enum FeedingPhase {
  FEED_FORWARD_PHASE,
  FEED_WAITING_PHASE,
  FEED_RETURN_PHASE
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

// Cutting state tracking
extern CuttingPhase currentCuttingPhase;

// Feeding state tracking
extern FeedingPhase currentFeedingPhase;
extern unsigned long feedReturnDelayStartTime;

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
bool isSystemIdle();
bool isSystemBusy();
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

// Individual state functions
void enterIdleState();
void updateIdleState();
void exitIdleState();

void enterCuttingState();
void updateCuttingState();
void exitCuttingState();

void enterReturningState();
void updateReturningState();
void exitReturningState();

void enterFeedingState();
void updateFeedingState();
void exitFeedingState();

void enterManualState();
void updateManualState();
void exitManualState();

#endif // STATEMACHINE_H 