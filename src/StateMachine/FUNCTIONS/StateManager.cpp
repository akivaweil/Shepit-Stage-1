#include "StateMachine.h"
#include "Config.h"
#include "Pins_Definitions.h"

//* ************************************************************************
//* ************************ STATE MANAGER IMPLEMENTATION ******************
//* ************************************************************************

// Include state class implementations
#include "../STATES/IdleState.cpp"

// Forward declarations for other state classes
class FeedToDistanceState;
class CuttingState;
class ReloadState;

// State objects - one instance of each
static IdleState idleState;
// We'll add other states as we implement them
// static FeedToDistanceState feedState;
// static CuttingState cuttingState;
// static ReloadState reloadState;

//* ************************************************************************
//* ************************ CONSTRUCTOR & DESTRUCTOR *********************
//* ************************************************************************

StateManager::StateManager() 
  : currentState(&idleState), 
    previousState(nullptr), 
    stateEntryTime(0), 
    stateChangeRequested(false), 
    pendingState(STATE_IDLE) {
  
  Serial.println("StateManager: Initialized");
}

StateManager::~StateManager() {
  // Clean up if needed
}

//* ************************************************************************
//* ************************ MAIN UPDATE FUNCTION *************************
//* ************************************************************************

void StateManager::update() {
  if (currentState) {
    // Let current state do its thing
    currentState->update();
    
    // Check for state timeout
    unsigned long timeout = currentState->getStateTimeout();
    if (timeout > 0 && (millis() - stateEntryTime) > timeout) {
      Serial.printf("StateManager: %s timeout - calling handleTimeout\n", 
                   currentState->getStateName());
      currentState->handleTimeout();
    }
    
    // Check for transition requests from the current state
    checkForStateTransitionRequests();
  }
  
  // Handle any pending state changes
  executeStateChange();
}

//* ************************************************************************
//* ************************ STATE TRANSITION MANAGEMENT ******************
//* ************************************************************************

void StateManager::transitionTo(SystemState newState) {
  if (currentState && currentState->canTransitionTo(newState)) {
    stateChangeRequested = true;
    pendingState = newState;
    
    Serial.printf("StateManager: State change requested: %s -> %s\n", 
                 currentState->getStateName(), getStateName(newState));
  } else {
    Serial.printf("StateManager: Invalid state transition requested: %s -> %s\n", 
                 currentState ? currentState->getStateName() : "UNKNOWN", 
                 getStateName(newState));
  }
}

void StateManager::executeStateChange() {
  if (stateChangeRequested) {
    Serial.printf("StateManager: Executing state change: %s -> %s\n", 
                 currentState->getStateName(), getStateName(pendingState));
    
    // Exit current state
    if (currentState) {
      currentState->exit();
      previousState = currentState;
    }
    
    // Switch to new state
    currentState = getStateObject(pendingState);
    
    // Enter new state
    if (currentState) {
      currentState->enter();
      stateEntryTime = millis();
      
      // Update global state variable for compatibility
      currentSystemState = currentState->getStateId();
      previousSystemState = previousState ? previousState->getStateId() : currentSystemState;
      
      Serial.printf("StateManager: Now in state: %s\n", currentState->getStateName());
    }
    
    stateChangeRequested = false;
  }
}

//* ************************************************************************
//* ************************ STATE INFORMATION ****************************
//* ************************************************************************

SystemState StateManager::getCurrentState() const {
  return currentState ? currentState->getStateId() : STATE_IDLE;
}

const char* StateManager::getCurrentStateName() const {
  return currentState ? currentState->getStateName() : "UNKNOWN";
}

State* StateManager::getCurrentStateObject() const {
  return currentState;
}

bool StateManager::isStateChangeRequested() const {
  return stateChangeRequested;
}

SystemState StateManager::getPendingState() const {
  return pendingState;
}

//* ************************************************************************
//* ************************ PRIVATE HELPER FUNCTIONS *********************
//* ************************************************************************

State* StateManager::getStateObject(SystemState state) {
  switch(state) {
    case STATE_IDLE: return &idleState;
    // We'll add other states as we implement them
    // case STATE_FEED_TO_DISTANCE: return &feedState;
    // case STATE_CUTTING: return &cuttingState;
    // case STATE_RELOAD: return &reloadState;
    default: 
      Serial.printf("StateManager: Unknown state %d, defaulting to IDLE\n", state);
      return &idleState;
  }
}

const char* StateManager::getStateName(SystemState state) {
  switch(state) {
    case STATE_IDLE: return "IDLE";
    case STATE_FEED_TO_DISTANCE: return "FEED_TO_DISTANCE";
    case STATE_CUTTING: return "CUTTING";
    case STATE_RELOAD: return "RELOAD";
    default: return "UNKNOWN";
  }
}

void StateManager::checkForStateTransitionRequests() {
  // Check if the current state has requested a transition
  if (currentState && currentState->hasTransitionRequest()) {
    SystemState requestedState = currentState->getRequestedState();
    Serial.printf("StateManager: %s requested transition to %s\n", 
                 currentState->getStateName(), getStateName(requestedState));
    
    // Request the transition
    transitionTo(requestedState);
    
    // Clear the request
    currentState->clearTransitionRequest();
  }
}
