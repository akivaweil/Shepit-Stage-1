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
  STATE_FEED_TO_DISTANCE = 1,
  STATE_CUTTING = 2,
  STATE_RETURNING = 3,
  STATE_MANUAL = 4
};

// Cutting phase enumeration (no longer used - kept for compatibility)
enum CuttingPhase {
  CUT_FORWARD_PHASE,
  CUT_BACKWARD_PHASE
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

// Pneumatic clamp control functions
void extendClamp();
void retractClamp();
bool isClampRetracted();

// Wood sensor functions
bool isWoodPresent();

// Wood distance sensor functions
bool isWoodAtCorrectDistance();

// Run cycle switch functions
bool isRunCycleSwitchActive();

// Continuous feed functions
void startContinuousFeed();
void stopContinuousFeed();

// Individual state functions
void enterIdleState();
void updateIdleState();
void exitIdleState();

void enterFeedToDistanceState();
void updateFeedToDistanceState();
void exitFeedToDistanceState();

void enterCuttingState();
void updateCuttingState();
void exitCuttingState();

// Cutting state phase functions
void updatePositioningPhase();
void updateCuttingPhase();

void enterReturningState();
void updateReturningState();
void exitReturningState();

void enterManualState();
void updateManualState();
void exitManualState();

#endif // STATEMACHINE_H 