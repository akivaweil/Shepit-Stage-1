#include <Arduino.h>
#include "Config.h"

//* ************************************************************************
//* *********************** MOTOR CONFIGURATION ****************************
//* ************************************************************************
// Feed motor parameters
const float feedMotorSpeed = 100.0;           // Steps per second
const float feedMotorAcceleration = 300.0;   // Steps per second^2 (increased for smoother acceleration)
const float feedMotorSteps = 50.0;           // Steps per move

// Cut motor parameters
const float cutMotorSpeed = 1000.0;            // Steps per second (reduced for smoother operation)
const float cutMotorAcceleration = 1500.0;    // Steps per second^2 (increased for smooth acceleration)
const float cutMotorSteps = 800.0;            // Steps per move

// Cut motor return parameters (for RETURNING state)
const float cutMotorReturnSpeed = 1000.0;      // Steps per second (slower for return movement)
const float cutMotorReturnAcceleration = 1500.0; // Steps per second^2 (gentler acceleration for return)

//* ************************************************************************
//* *********************** BUTTON CONFIGURATION ***************************
//* ************************************************************************
// Button debounce time in milliseconds
const unsigned long buttonDebounceTime = 20; 