# Project Rules & Guidelines

## 🚀 Version Control & Git Workflow
- **ALWAYS** push changes at the beginning of each chat thread with message "Before making x change"
- Create a new branch for all changes made in each chat session
- **ALWAYS** upload code (`pio run -t upload`) prior to commit and pushing
- Commit and push after every long chat session
- Keep commit descriptions short and to the point

## 📁 Project Structure & Organization

### Folder Structure
- Header files (`.h`) belong in `include/` folder
- Source files (`.cpp`) belong in `src/` folder
- Do not create new files unless specifically requested

### State Machine Organization
- All states should be in a folder labeled `StateMachine`
- Inside StateMachine should be:
  - `STATES/` folder (main state files should be UPPERCASED)
  - `FUNCTIONS/` folder
- Use numbering system for files: `00_`, `01_` prefixes to keep order
- The waiting state should be called "IDLE" state

### Required Configuration
- `src/Config/` folder must contain:
  - `Config.cpp`
  - `Pins_Definitions.cpp`

## 🔧 Hardware Configuration

### Network Settings
- **SSID**: Everwood
- **Password**: Everwood-Staff

### Input Logic
- **Physical switches**: Active HIGH (input pulldown) unless specified otherwise
- **Sensors**: Active LOW (input pullup) unless specified otherwise

### Common Parts Used
- Freenove ESP32 and ESP32-S3
- Stepper motors (typically NEMA 23, closed loop for high speed)
- Pneumatic solenoids
- Pneumatic cylinders
- 5V relays

## 💻 Code Standards & Best Practices

### Data Types
- Always use `float` instead of `int`

### Libraries
- **Bounce2 Library**: Always use `.read()` instead of `.rose()` unless specifically necessary
- **OTA Manager**: Always add `OTA_Manager.cpp` to projects for OTA uploads
  - Do NOT create header file - keep all code inside the `.cpp` file
- Do NOT use servo library - ask for custom one if needed

### Upload Process
- **ALWAYS** upload after making code changes: `pio run -t upload`
- Do NOT switch to USB uploads or modify upload methods if failures occur
- Stick to established upload process regardless of errors

## 📝 Documentation & Comments

### Comment Standards
- Use BetterComments VSCode extension
- Use `//!` to mark sequence steps
- Add section banners for each code section, especially state machine states

### Section Banner Format
```cpp
//* ************************************************************************
//* ************************ HOMING ***************************
//* ************************************************************************
```

### Sequence Step Format
```cpp
//! ************************************************************************
//! STEP 1: RETURN CUT MOTOR TO 0 AND RETRACT SECURE CLAMP (SIMULTANEOUS)
//! ************************************************************************
```

## 🔍 Code Safety & Variables

### Critical Rules
- NEVER assume variable names exist - always verify before use
- Do NOT add error states, timeouts, or error messages unless explicitly requested
- No additional features or modifications beyond what's specifically asked for

### Debugging
- **Critical**: Ensure no serial output occurs while motors are running
- Don't add serial logs unless specified
- Add lots of `//` comments instead

## 🎨 Folder Colorization

### Color Scheme
- 🏠 **Main (blue)** - Core application files
- ⚙️ **Functionality/Config (green)** - Functional components  
- 🔄 **Motors/StateMachine (purple/yellow)** - Movement and state logic
- 🎨 **Painting Sides (cyan)** - Painting-specific code
- 🔌 **Hardware (orange)** - Hardware interfaces
- 💾 **Storage (purple)** - Data persistence

### How to Modify
Add new objects to `folder-color.pathColors` array:
```json
{
    "folderPath": "src/Commands/",
    "color": "foldercolorizer.color_ff0000",
    "badge": "📝"
}
```

## 🏗️ Terminology & Language

### Pneumatic Components
- **Cylinders**: Use "extended" or "retracted" (NOT "engaged" or "disengaged")
- **Clamps**: Use "extended" or "retracted" (NOT "engaged" or "disengaged")

### Sensors & Logic
- **Active**: Means the sensor is triggered/activated

## 🚫 What NOT to Do

### Code Modifications
- Don't add error handling unless explicitly requested
- Don't create new files without permission
- Don't assume variable existence
- Don't add serial logging unless specified
- Don't use servo library (use custom one instead)

### Upload Process
- Don't switch upload methods on failure
- Don't modify established upload processes

## ✅ What TO Do

### Always Required
- Upload after code changes
- Use proper folder structure
- Follow naming conventions
- Add comprehensive comments
- Use BetterComments extension
- Colorize folders appropriately
- Follow version control workflow

### Code Quality
- Verify all variables before use
- Use proper data types (float over int)
- Follow established patterns
- Research library documentation for best practices
