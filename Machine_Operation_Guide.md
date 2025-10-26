# Shepit S1 Cubes - Machine Operation Guide

## Overview

The Shepit S1 Cubes is an automated wood cutting machine controlled by an ESP32 microcontroller. The system uses two stepper motors (feed and cut), pneumatic clamps, and multiple sensors to perform precision wood cutting operations.

---

## Hardware Components

### Motors
- **Feed Motor**: Continuously feeds wood forward during operation
  - Step Pin: 4
  - Direction Pin: 5
  - Enable Pin: 6
  - Speed: 3000 Hz
  - Acceleration: 25000 steps/s²
  
- **Cut Motor**: Performs the actual cutting motion
  - Step Pin: 15
  - Direction Pin: 16
  - Enable Pin: 17
  - Cutting Speed: 850 Hz
  - Cutting Acceleration: 30000 steps/s²
  - Return Speed: 30000 Hz
  - Return Acceleration: 35000 steps/s²
  - Cut Steps: 3500 steps per cut

### Pneumatic Clamp
- **Relay Pin**: 37
- **Logic**: LOW = Extended (secure), HIGH = Retracted (free)
- Used to secure wood during cutting operations

### Sensors

#### Input Switches (Active HIGH - INPUT_PULLDOWN)
- **Run Cycle Switch** (Pin 11): Enables automatic cutting cycle
- **Reload Switch** (Pin 41): Manually triggers reload mode
- **Red Button** (Pin 39): Emergency stop button
- **Main Button** (Pin 9): Starts sequences or triggers emergency stop

#### Wood Sensors (Active LOW - INPUT_PULLUP)
- **Wood Presence Sensor** (Pin 3): Detects if wood is present
- **Wood Distance Sensor** (Pin 10): Detects when wood reaches correct cutting position

#### Cut Motor Home Switch (Active HIGH - INPUT_PULLDOWN)
- **Cut Motor Home Switch** (Pin 1): Indicates cut motor is at home position

### Safety Features
- **Motor Sleep Mode**: Motors automatically disable after 2 seconds of inactivity in IDLE state
- **Feed Motor Timeout**: 5-second safety limit for feed motor operation
- **Cut Motor Homing Safety**: Feed motor cannot operate unless cut motor is homed
- **Emergency Stop**: 300ms delay prevents accidental stops immediately after cycle start
- **Startup Safety**: Requires run cycle switch to be cycled if ON at startup

---

## State Machine States

### 1. IDLE State (00_IDLE.cpp)
**Purpose**: Waiting state where the machine monitors conditions and controls continuous feeding

**Key Behaviors**:
- **Continuous Feed Operation**: Feed motor runs forward when:
  - Run cycle switch is ACTIVE
  - Wood is present
  - NOT in cutting cycle
  - NOT timeout locked
  - NOT in reload state
- **Clamp Control**: Automatically retracts clamp when feed motor runs, extends when stopped
- **Timeout Protection**: After 5 seconds of continuous feed, locks motor until manually reset
- **Transition Conditions**:
  - Distance sensor triggered → **CUTTING** state
  - Reload switch activated → **RELOAD** state
  - Button pressed + run cycle active + wood present → **HOMING** state

**Auto-Homing Safety**: If cut motor not homed when feed should start, automatically homes before allowing feed

### 2. HOMING State (01_HOMING.cpp)
**Purpose**: Ensures cut motor is at a known home position before cutting operations

**Operation**:
- Moves cut motor backward at 400 Hz toward home switch
- Stops when home switch is triggered
- Sets cut motor position to 0
- Marks cut motor as "homed" for safety

**Transition**: Returns to specified state after homing completes

### 3. FEED_TO_DISTANCE State (02_FEED_TO_DISTANCE.cpp)
**Purpose**: Feeds wood forward until distance sensor is triggered

**Operation**:
- Retracts forward clamp
- Starts feed motor forward
- Monitors wood presence throughout feed
- Waits 10ms after distance sensor trigger (woodDistanceDelay)
- Stops feed motor and extends clamp when distance reached

**Special Handling**:
- **Wood Loss Detection**: If wood disappears during feed:
  - Stops feed motor
  - Automatically reloads 5000 steps backward
  - Waits for wood to return and restart
- **Timeout Protection**: 5-second limit on feed operation

**Transitions**:
- On successful feed → **CUTTING** state
- On timeout/error → **IDLE** state (with timeout lock)

### 4. CUTTING State (03_CUTTING.cpp)
**Purpose**: Performs the actual cutting operation

**Three-Step Process**:

**Step 1: Cut Wood**
- Moves cut motor forward 3500 steps at 850 Hz
- Waits for movement to complete
- Safety timeout: 10 seconds

**Step 2: Return Cut Motor**
- Returns cut motor to home position (position 0)
- Speed: 30000 Hz
- Acceleration: 35000 steps/s²
- Safety timeout: 5 seconds

**Step 3: Check Conditions**
- **If wood still present AND run cycle active**: → **HOMING** state (then FEED_TO_DISTANCE)
- **If wood present BUT run cycle inactive**: → **IDLE** state
- **If wood NOT present**: → **RELOAD** state

### 5. RELOAD State (04_RELOAD.cpp)
**Purpose**: Manual reload mode to move wood backward

**Operation**:
- Moves feed motor backward 5000 steps
- Requires reload switch to be active to enter
- Exits if reload switch deactivated during operation
- Extends forward clamp when complete

**Transition**: Returns to **IDLE** state

---

## Normal Operation Flow

### Startup Sequence
1. System powers on
2. Motors initialize and enable
3. All sensors initialize
4. Startup safety check (if run cycle switch ON, requires reset)
5. System enters **HOMING** state (ensures cut motor position known)
6. After homing, enters **IDLE** state

### Automatic Cutting Cycle

**Trigger**: Button press when:
- Run cycle switch is ACTIVE
- Wood is detected present

**Sequence**:
1. **HOMING**: Ensure cut motor is at home position
2. **FEED_TO_DISTANCE**: Feed wood forward until distance sensor triggered
3. **CUTTING**: Perform cut, return cut motor, check conditions
4. **Repeat** or **IDLE** based on conditions

### Continuous Feed Operation (IDLE State)

When run cycle switch is ON and wood is present:
- Feed motor runs continuously forward
- Forward clamp automatically retracts
- Wood advances toward cut position
- System monitors for distance sensor trigger

When distance sensor triggers:
- Feed motor stops
- Forward clamp extends
- Transitions to CUTTING state

---

## Safety Mechanisms

### 1. Cut Motor Homing Safety
- **Purpose**: Prevents uncontrolled cutting if cut motor position unknown
- **Implementation**: Feed motor operations blocked unless cut motor homed
- **Auto-Recovery**: System automatically homes cut motor when needed

### 2. Feed Motor Timeout Lock
- **Purpose**: Prevents infinite feeding if distance sensor fails
- **Duration**: 5 seconds maximum feed time
- **Behavior**: After timeout, locks feed motor until manually reset
- **Reset Method**: Turn run cycle switch OFF then ON

### 3. Motor Sleep Mode
- **Purpose**: Reduces motor heating and power consumption
- **Timing**: Motors disabled after 2 seconds of inactivity in IDLE state
- **Auto-Wake**: Motors automatically re-enable when movement needed

### 4. Emergency Stop
- **Trigger**: Button press during active operation (after 300ms delay)
- **Action**: 
  - Immediately stops all motors
  - Resets cut motor homed flag
  - Returns to IDLE state

### 5. Wood Loss Detection
- **Detection**: Monitors wood presence during feed operation
- **Response**: Automatically reloads wood backward and waits for replacement

### 6. Startup Safety
- **Check**: Verifies run cycle switch state at startup
- **If ON**: Requires user to turn OFF then ON to reset
- **Prevents**: Automatic cycle start without user intervention

---

## Serial Commands

### Motor Control
- `enablefeed` / `disablefeed` - Enable/disable feed motor
- `enablecut` / `disablecut` - Enable/disable cut motor
- `disableall` - Disable all motors (sleep mode)

### Movement
- `feedforward` / `feedbackward` - Move feed motor forward/backward
- `cutforward` / `cutbackward` - Move cut motor forward/backward
- `feedstop` / `cutstop` - Stop respective motor
- `feed500` / `cut-200` - Move specific number of steps
- `feedspeed500` / `cutspeed100` - Set motor speeds

### Clamp Control
- `clampextend` / `clampretract` - Control pneumatic clamp

### System Control
- `status` - Show comprehensive system status
- `sequence` - Start cutting sequence manually
- `reload` - Start reload mode
- `stopreload` - Stop reload mode
- `stop` / `emergency` - Emergency stop
- `idle` - Return to IDLE state
- `help` - Show available commands

---

## Configuration Parameters

Located in `src/Config/Config.cpp`:

### Motor Parameters
- Feed Motor Speed: 3000 Hz
- Feed Motor Acceleration: 25000 steps/s²
- Cut Motor Speed: 850 Hz
- Cut Motor Acceleration: 30000 steps/s²
- Cut Motor Return Speed: 30000 Hz
- Cut Motor Return Acceleration: 35000 steps/s²
- Cut Motor Steps: 3500 steps per cut

### Timing Parameters
- Wood Distance Delay: 10ms
- Feed Motor Timeout: 5000ms (5 seconds)
- Motor Sleep Timeout: 2000ms (2 seconds)
- Motor Enable Delay: 250ms
- Button Debounce: 20ms
- Sensor Debounce: 2ms
- Distance Sensor Debounce: 20ms

### Reload Parameters
- Reload Steps: 5000 steps backward
- Automatic Reload Steps: 5000 steps backward

---

## State Transitions Diagram

```
STARTUP
  ↓
HOMING ───────────────────────────────────┐
  ↓                                      │
IDLE ◄───────────────────────────────────┤
  │                                      │
  ├──[Button + Run Cycle + Wood]──► HOMING ──► FEED_TO_DISTANCE
  │                                                              │
  ├──[Distance Sensor]─────────────────────────────────► CUTTING
  │                                                              │
  └──[Reload Switch]──► RELOAD ──► IDLE                       │
                                                                 │
                                                         ┌───────▼───────┐
                                                         │ Check Conditions │
                                                         └───────┬───────┘
                                                                 │
                  ┌─────────────────────────────────────────────┼─────────────┐
                  │                                             │             │
         [Wood Present                                    [Wood Present  [No Wood]
          + Run Cycle Active]                             + Run OFF]            │
                  │                                             │             │
                  ▼                                             ▼             ▼
               HOMING ──► FEED_TO_DISTANCE                   IDLE         RELOAD
```

---

## Troubleshooting

### Feed Motor Won't Start
- Check: Is cut motor homed? (system will auto-home if needed)
- Check: Is run cycle switch active?
- Check: Is wood present?
- Check: Is feed motor timeout locked? (cycle run switch OFF/ON to reset)

### Cut Motor Won't Home
- Check: Home switch is functioning (Pin 1)
- Check: Motors are enabled
- Check: Switch wiring (active HIGH)

### Wood Sensor Issues
- Wood presence sensor (Pin 3) - Active LOW
- Distance sensor (Pin 10) - Active LOW
- Check sensor wiring and pullup resistors

### Clamp Not Moving
- Check: Relay wiring on Pin 37
- Check: Pneumatic system power
- Logic: LOW = Extended, HIGH = Retracted

---

## Technical Notes

### Motor Enable Logic
- Motors use **active LOW** enable pins
- LOW signal = motor enabled
- HIGH signal = motor disabled (sleep mode)

### Sensor Logic
- **Active HIGH**: Run cycle switch, reload switch, red button, cut motor home switch
- **Active LOW**: Wood presence sensor, wood distance sensor

### Debouncing
- Switches and buttons use Bounce2 library
- Distance sensor uses 20ms debounce for reliability
- Cut motor home switch uses direct digital reading (no debounce)

### Safety Priority
All operations prioritize safety by checking:
1. Motor position known (homing)
2. Wood presence confirmed
3. Timeout protection active
4. Emergency stop available

