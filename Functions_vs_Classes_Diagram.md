# Functions vs Classes - Visual Diagram

## 🎯 FUNCTIONS (Standalone Code Blocks)

```
┌─────────────────────────────────────────┐
│           FUNCTION                      │
├─────────────────────────────────────────┤
│  Input Parameters                       │
│  ┌─────────┐  ┌─────────┐              │
│  │   a     │  │   b     │              │
│  └─────────┘  └─────────┘              │
├─────────────────────────────────────────┤
│  Code Logic                            │
│  ┌─────────────────────────────────────┐│
│  │  float result = a + b;             ││
│  │  return result;                     ││
│  └─────────────────────────────────────┘│
├─────────────────────────────────────────┤
│  Return Value                          │
│  ┌─────────┐                           │
│  │ result  │                           │
│  └─────────┘                           │
└─────────────────────────────────────────┘

Usage: addNumbers(5, 3) → returns 8
```

## 🏗️ CLASSES (Object Blueprints)

```
┌─────────────────────────────────────────────────────────────┐
│                    CLASS: StepperMotor                     │
├─────────────────────────────────────────────────────────────┤
│  PRIVATE DATA (Variables)                                  │
│  ┌─────────────────────────────────────────────────────────┐│
│  │  int stepPin;                                          ││
│  │  int dirPin;                                           ││
│  │  int currentPosition;                                  ││
│  └─────────────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────────────┤
│  PUBLIC METHODS (Functions)                                │
│  ┌─────────────────────────────────────────────────────────┐│
│  │  StepperMotor(int step, int dir)  ← Constructor       ││
│  │  void moveSteps(int steps)        ← Method            ││
│  │  int getPosition()                 ← Method            ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘

Usage: StepperMotor motor1(2, 3); motor1.moveSteps(100);
```

## 🔄 YOUR PROJECT ARCHITECTURE

```
┌─────────────────────────────────────────────────────────────────┐
│                    STATEMACHINE SYSTEM                         │
├─────────────────────────────────────────────────────────────────┤
│                                                               │
│  📁 FUNCTIONS/                    📁 STATES/                  │
│  ┌─────────────────────────┐      ┌─────────────────────────┐ │
│  │  Sensor_Setup.cpp       │      │  00_IDLE.cpp            │ │
│  │  ┌─────────────────┐    │      │  ┌─────────────────┐    │ │
│  │  │ setupSensors()  │    │      │  │ class IdleState │    │ │
│  │  │ readSensor()    │    │      │  │ ┌─────────────┐  │    │ │
│  │  └─────────────────┘    │      │  │ │ enter()     │  │    │ │
│  │                         │      │  │ │ execute()   │  │    │ │
│  │  StateMachine_          │      │  │ │ exit()      │  │    │ │
│  │  Functions.cpp          │      │  │ └─────────────┘  │    │ │
│  │  ┌─────────────────┐    │      │  └─────────────────┘    │ │
│  │  │ changeState()   │    │      │                         │ │
│  │  │ updateSystem()  │    │      │  04_RELOAD.cpp          │ │
│  │  └─────────────────┘    │      │  ┌─────────────────┐    │ │
│  └─────────────────────────┘      │  │ class ReloadState│    │ │
│                                   │  │ ┌─────────────┐  │    │ │
│                                   │  │ │ enter()     │  │    │ │
│                                   │  │ │ execute()   │  │    │ │
│                                   │  │ │ exit()      │  │    │ │
│                                   │  │ └─────────────┘  │    │ │
│                                   │  └─────────────────┘    │ │
│                                   └─────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

## 📊 COMPARISON TABLE

| Aspect | Functions | Classes |
|--------|-----------|---------|
| **Purpose** | Single task | Group of related tasks |
| **Data** | No persistent data | Can store data |
| **Usage** | `functionName()` | `object.methodName()` |
| **Instances** | One copy | Multiple objects |
| **Memory** | Stack-based | Heap-based objects |
| **Example** | `moveMotor(100)` | `motor1.moveSteps(100)` |

## 🎨 REAL-WORLD ANALOGY

**Functions = Tools in a Toolbox**
- Hammer: `hammerNail()`
- Screwdriver: `turnScrew()`
- Each tool does one job

**Classes = Complete Machines**
- Car: has engine, wheels, methods like `start()`, `drive()`
- Robot: has sensors, motors, methods like `move()`, `detect()`

## 🔧 IN YOUR CUBE CUTTING MACHINE

**Functions handle:**
- Sensor readings
- Motor control commands
- State transitions
- Utility calculations

**Classes handle:**
- Each cutting state (IDLE, FEED, CUT, RELOAD)
- Motor objects with position tracking
- Sensor objects with calibration data
- State machine coordination

This architecture gives you clean separation of concerns and makes your code modular and maintainable!
