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
//* *********************** STATE MACHINE *********************************
//* ************************************************************************
// State enumeration
enum SystemState {
  STATE_IDLE = 0,
  STATE_CUTTING = 1,
  STATE_FEEDING = 2,
  STATE_MANUAL = 3
};

// State machine variables
SystemState currentSystemState = STATE_IDLE;
SystemState previousSystemState = STATE_IDLE;
unsigned long lastActivityTime = 0;
bool motorsEnabled = false;
bool manualMode = false;
const unsigned long MOTOR_TIMEOUT_MS = 2000; // 2 seconds

// Cutting state tracking
enum CuttingPhase {
  CUT_FORWARD_PHASE,
  CUT_BACKWARD_PHASE
};
CuttingPhase currentCuttingPhase = CUT_FORWARD_PHASE;

//* ************************************************************************
//* *********************** SERIAL COMMAND PROCESSING *********************
//* ************************************************************************
String inputString = "";
bool stringComplete = false;

//* ************************************************************************
//* *********************** MOTOR ENABLE FUNCTIONS ************************
//* ************************************************************************

void resetMotorTimeout() {
  lastActivityTime = millis();
}

void enableAllMotors() {
  if (!motorsEnabled) {
    digitalWrite(FEED_MOTOR_ENABLE_PIN, LOW);  // Active low enable
    digitalWrite(CUT_MOTOR_ENABLE_PIN, LOW);   // Active low enable
    motorsEnabled = true;
    Serial.println("All motors ENABLED");
  }
  resetMotorTimeout();
}

void disableAllMotorsAfterDelay() {
  if (motorsEnabled) {
    digitalWrite(FEED_MOTOR_ENABLE_PIN, HIGH); // Active low enable
    digitalWrite(CUT_MOTOR_ENABLE_PIN, HIGH);  // Active low enable
    motorsEnabled = false;
    Serial.println("All motors DISABLED due to timeout");
  }
}

void checkMotorTimeout() {
  // Only check timeout in idle state
  if (currentSystemState == STATE_IDLE) {
    if (motorsEnabled && (millis() - lastActivityTime >= MOTOR_TIMEOUT_MS)) {
      disableAllMotorsAfterDelay();
    }
  }
}

// Individual motor control functions maintained for manual commands
void enableFeedMotor() {
  digitalWrite(FEED_MOTOR_ENABLE_PIN, LOW);  // Active low enable
  resetMotorTimeout();
}

void disableFeedMotor() {
  digitalWrite(FEED_MOTOR_ENABLE_PIN, HIGH); // Active low enable
}

void enableCutMotor() {
  digitalWrite(CUT_MOTOR_ENABLE_PIN, LOW);   // Active low enable
  resetMotorTimeout();
}

void disableCutMotor() {
  digitalWrite(CUT_MOTOR_ENABLE_PIN, HIGH);  // Active low enable
}

//* ************************************************************************
//* *********************** STATE MACHINE FUNCTIONS **********************
//* ************************************************************************

String getCurrentStateName() {
  switch (currentSystemState) {
    case STATE_IDLE: return "IDLE";
    case STATE_CUTTING: return "CUTTING";
    case STATE_FEEDING: return "FEEDING";
    case STATE_MANUAL: return "MANUAL";
    default: return "UNKNOWN";
  }
}

bool isSystemIdle() {
  return currentSystemState == STATE_IDLE;
}

bool isSystemBusy() {
  return (currentSystemState == STATE_CUTTING || 
          currentSystemState == STATE_FEEDING);
}

void transitionToState(SystemState newState) {
  if (newState != currentSystemState) {
    Serial.println("*** TRANSITIONING FROM " + getCurrentStateName() + " TO " + 
                   (newState == STATE_IDLE ? "IDLE" : 
                    newState == STATE_CUTTING ? "CUTTING" : 
                    newState == STATE_FEEDING ? "FEEDING" : 
                    newState == STATE_MANUAL ? "MANUAL" : "UNKNOWN") + " ***");
    
    previousSystemState = currentSystemState;
    currentSystemState = newState;
    
    // Reset activity timer on state change
    resetMotorTimeout();
    
    // Handle state-specific initialization
    switch (currentSystemState) {
      case STATE_IDLE:
        manualMode = false;
        Serial.println("System ready - waiting for button press or manual command");
        Serial.println("Motors will disable after 2 seconds of inactivity");
        break;
        
      case STATE_CUTTING:
        enableAllMotors();
        currentCuttingPhase = CUT_FORWARD_PHASE;
        if (cutMotor) {
          Serial.println("Starting cut motor forward movement (" + String(cutMotorSteps) + " steps)");
          cutMotor->move(cutMotorSteps);
        }
        break;
        
      case STATE_FEEDING:
        enableAllMotors();
        if (feedMotor) {
          Serial.println("Starting feed motor forward movement (" + String(feedMotorSteps) + " steps)");
          feedMotor->move(feedMotorSteps);
        }
        break;
        
      case STATE_MANUAL:
        enableAllMotors();
        manualMode = true;
        Serial.println("Manual mode active - motors enabled");
        break;
    }
  }
}

void updateStateMachine() {
  // Update the current state
  switch (currentSystemState) {
    case STATE_IDLE:
      // Check for motor timeout (2 seconds of inactivity)
      checkMotorTimeout();
      break;
      
    case STATE_CUTTING:
      // Reset activity timer to keep motors enabled
      resetMotorTimeout();
      
      // Handle cutting phases
      switch (currentCuttingPhase) {
        case CUT_FORWARD_PHASE:
          // Check if cut motor forward movement is complete
          if (cutMotor && !cutMotor->isRunning()) {
            Serial.println("Cut motor forward movement COMPLETE");
            
            // Move to backward phase
            currentCuttingPhase = CUT_BACKWARD_PHASE;
            
            // Start cut motor backward movement
            Serial.println("Starting cut motor backward movement (" + String(cutMotorSteps) + " steps)");
            cutMotor->move(-cutMotorSteps);
          }
          break;
          
        case CUT_BACKWARD_PHASE:
          // Check if cut motor backward movement is complete
          if (cutMotor && !cutMotor->isRunning()) {
            Serial.println("Cut motor backward movement COMPLETE");
            
            // Cutting sequence complete, transition to feeding
            transitionToState(STATE_FEEDING);
          }
          break;
      }
      break;
      
    case STATE_FEEDING:
      // Reset activity timer to keep motors enabled
      resetMotorTimeout();
      
      // Check if feed motor movement is complete
      if (feedMotor && !feedMotor->isRunning()) {
        Serial.println("Feed motor forward movement COMPLETE");
        
        // Feeding sequence complete, return to idle
        Serial.println("*** CUTTING CYCLE COMPLETE ***");
        transitionToState(STATE_IDLE);
      }
      break;
      
    case STATE_MANUAL:
      // Reset activity timer to keep motors enabled during manual operations
      resetMotorTimeout();
      break;
  }
}

void initializeStateMachine() {
  Serial.println("=== INITIALIZING STATE MACHINE ===");
  
  // Initialize variables
  lastActivityTime = millis();
  motorsEnabled = false;
  manualMode = false;
  
  // Start in idle state
  currentSystemState = STATE_IDLE;
  previousSystemState = STATE_IDLE;
  
  Serial.println("State machine initialized - starting in IDLE state");
}

//* ************************************************************************
//* *********************** SERIAL COMMAND HANDLER ************************
//* ************************************************************************
void processSerialCommand(String command) {
  command.trim();
  command.toLowerCase();
  
  Serial.println("Command received: " + command);
  
  // Transition to manual mode for motor commands
  if (!manualMode && (command.startsWith("feed") || command.startsWith("cut") || 
                      command == "enablefeed" || command == "disablefeed" ||
                      command == "enablecut" || command == "disablecut")) {
    transitionToState(STATE_MANUAL);
  }
  
  // Motor enable/disable commands (legacy support)
  if (command == "enablefeed") {
    enableAllMotors();
    Serial.println("Feed motor ENABLED via manual command");
  }
  else if (command == "disablefeed") {
    disableFeedMotor();
    Serial.println("Feed motor DISABLED via manual command");
  }
  else if (command == "enablecut") {
    enableAllMotors();
    Serial.println("Cut motor ENABLED via manual command");
  }
  else if (command == "disablecut") {
    disableCutMotor();
    Serial.println("Cut motor DISABLED via manual command");
  }
  else if (command == "disableall") {
    disableAllMotorsAfterDelay();
  }
  
  // Feed motor movement commands
  else if (command == "feedforward") {
    if (feedMotor) {
      enableAllMotors();
      feedMotor->move(feedMotorSteps);
      Serial.println("Feed motor moving forward");
    }
  }
  else if (command == "feedbackward") {
    if (feedMotor) {
      enableAllMotors();
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
      enableAllMotors();
      cutMotor->move(cutMotorSteps);
      Serial.println("Cut motor moving forward");
    }
  }
  else if (command == "cutbackward") {
    if (cutMotor) {
      enableAllMotors();
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
      enableAllMotors();
      feedMotor->move(steps);
      Serial.println("Feed motor moving " + String(steps) + " steps");
    }
  }
  else if (command.startsWith("cut")) {
    String stepStr = command.substring(3);
    float steps = stepStr.toFloat();
    if (cutMotor && steps != 0) {
      enableAllMotors();
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
    Serial.println("Current state: " + getCurrentStateName());
    Serial.println("Motors enabled: " + String(motorsEnabled));
    Serial.println("Manual mode: " + String(manualMode));
    Serial.println("Feed motor running: " + String(feedMotor ? feedMotor->isRunning() : false));
    Serial.println("Cut motor running: " + String(cutMotor ? cutMotor->isRunning() : false));
    Serial.println("Feed motor position: " + String(feedMotor ? feedMotor->getCurrentPosition() : 0));
    Serial.println("Cut motor position: " + String(cutMotor ? cutMotor->getCurrentPosition() : 0));
    Serial.println("Last activity: " + String(millis() - lastActivityTime) + "ms ago");
  }
  
  // Emergency stop
  else if (command == "stop" || command == "emergency") {
    if (feedMotor) feedMotor->forceStop();
    if (cutMotor) cutMotor->forceStop();
    disableAllMotorsAfterDelay();
    transitionToState(STATE_IDLE);
    Serial.println("EMERGENCY STOP - All motors stopped and disabled");
  }
  
  // Run sequence manually
  else if (command == "sequence") {
    if (isSystemIdle()) {
      Serial.println("Starting manual sequence...");
      transitionToState(STATE_CUTTING);
    } else {
      Serial.println("Sequence already running - current state: " + getCurrentStateName());
    }
  }
  
  // Return to idle from manual mode
  else if (command == "idle") {
    transitionToState(STATE_IDLE);
    Serial.println("Returning to idle state");
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
  //! STEP 3: INITIALIZE ENABLE PINS AND STATE MACHINE
  //! ************************************************************************
  Serial.println("Setting up motor enable pins...");
  pinMode(FEED_MOTOR_ENABLE_PIN, OUTPUT);
  pinMode(CUT_MOTOR_ENABLE_PIN, OUTPUT);
  
  // Initialize state machine
  Serial.println("Initializing state machine...");
  initializeStateMachine();

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
    // Enable auto-enable for smoother operation
    feedMotor->setAutoEnable(true);
    // Set current position to 0 for reference
    feedMotor->setCurrentPosition(0);
    Serial.println("Feed motor configured successfully");
    Serial.println("Feed motor speed: " + String(feedMotorSpeed) + " Hz");
    Serial.println("Feed motor acceleration: " + String(feedMotorAcceleration) + " steps/s²");
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
    // Enable auto-enable for smoother operation
    cutMotor->setAutoEnable(true);
    // Set current position to 0 for reference
    cutMotor->setCurrentPosition(0);
    Serial.println("Cut motor configured successfully");
    Serial.println("Cut motor speed: " + String(cutMotorSpeed) + " Hz");
    Serial.println("Cut motor acceleration: " + String(cutMotorAcceleration) + " steps/s²");
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
  if (button.pressed() && isSystemIdle()) {
    Serial.println("*** BUTTON PRESSED - STARTING SEQUENCE ***");
    transitionToState(STATE_CUTTING);
  }

  //! ************************************************************************
  //! STEP 5: UPDATE STATE MACHINE
  //! ************************************************************************
  updateStateMachine();

  //! ************************************************************************
  //! STEP 6: SMALL DELAY TO PREVENT WATCHDOG ISSUES
  //! ************************************************************************
  delay(10);
}
