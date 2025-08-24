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
  STATE_RELOAD = 3
};

// System events for state machine
enum SystemEvent {
  EVENT_WOOD_DETECTED,
  EVENT_DISTANCE_SENSOR_TRIGGERED,
  EVENT_CUT_COMPLETE,
  EVENT_EMERGENCY_STOP,
  EVENT_MOTOR_TIMEOUT,
  EVENT_RELOAD_REQUESTED,
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
//* *********************** STATE PATTERN CLASSES *************************
//* ************************************************************************

// Forward declarations
class State;
class StateManager;

// Base State class - all states inherit from this
class State {
public:
  virtual ~State() = default;
  
  // Core state lifecycle methods
  virtual void enter() = 0;                    // Called when entering state
  virtual void update() = 0;                   // Called every loop iteration
  virtual void exit() = 0;                     // Called when leaving state
  
  // Event handling for state-specific responses
  virtual void handleEvent(SystemEvent event) = 0;
  
  // State identification
  virtual SystemState getStateId() const = 0;
  virtual const char* getStateName() const = 0;
  
  // State-specific safety checks
  virtual bool canTransitionTo(SystemState targetState) const = 0;
  
  // State-specific timeout handling
  virtual unsigned long getStateTimeout() const = 0;
  virtual void handleTimeout() = 0;
  
  // State transition request handling
  virtual bool hasTransitionRequest() const = 0;
  virtual SystemState getRequestedState() const = 0;
  virtual void clearTransitionRequest() = 0;
};

// State Manager class - handles state transitions and current state
class StateManager {
private:
  State* currentState;
  State* previousState;
  unsigned long stateEntryTime;
  bool stateChangeRequested;
  SystemState pendingState;
  
public:
  StateManager();
  ~StateManager();
  
  // Main update function
  void update();
  
  // State transition management
  void transitionTo(SystemState newState);
  void executeStateChange();
  
  // State information
  SystemState getCurrentState() const;
  const char* getCurrentStateName() const;
  State* getCurrentStateObject() const;
  
  // State change tracking
  bool isStateChangeRequested() const;
  SystemState getPendingState() const;
  
private:
  State* getStateObject(SystemState state);
  const char* getStateName(SystemState state);
  void checkForStateTransitionRequests();
};

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

// Global state manager instance
extern StateManager stateMachine;

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
void resetFeedMotorControlVariables();

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
void initializeReloadSwitch();
void initializeRedButton();
void initializeAllSensors();

// Sensor update functions
void updateFeedDistanceSensor();
void updateWoodPresenceSensor();
void updateRunCycleSwitch();
void updateReloadSwitch();
void updateRedButton();
void updateAllSensors();

// Sensor read functions
bool isFeedDistanceSensorTriggered();
bool isWoodPresenceSensorActive();
bool isRunCycleSwitchActive();
bool isRunCycleSwitchActiveCentralized();
bool isReloadSwitchActive();
bool isRedButtonPressed();

// Sensor reset functions
void resetFeedDistanceSensor();
void resetWoodPresenceSensor();
void resetRunCycleSwitch();
void resetReloadSwitch();
void resetRedButton();
void resetAllSensors();

// External references to sensor objects
extern Bounce2::Button feedDistanceSensor;
extern Bounce2::Button woodPresenceSensor;
extern Bounce2::Button runCycleSwitch;
extern Bounce2::Button reloadSwitch;
extern Bounce2::Button redButton;

// Continuous feed functions
void startContinuousFeed();
void stopContinuousFeed();

// Emergency stop function for feed operations
void emergencyStopFeedOperation();

// Reload mode state
extern bool reloadModeActive;

// Individual state functions (legacy - will be replaced by State classes)
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
void updateRetractForwardClampStep();
void updateFeedForwardStep();
void updateExtendForwardClampStep();
void updateCutWoodStep();
void updateReturnCutMotorStep();
void updateCheckConditionsStep();

// Cutting cycle flag reset functions
void resetCutMotorStepFlags();
void resetReturnMotorStepFlags();

void enterReloadState();
void updateReloadState();
void exitReloadState();

// Distance sensor functions
void initializeFeedDistanceSensor();
void updateFeedDistanceSensor();
bool isFeedDistanceSensorTriggered();
void resetFeedDistanceSensor();

#endif // STATEMACHINE_H 