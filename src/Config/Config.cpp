#include <Arduino.h>
#include "Config.h"

//* ************************************************************************
//* *********************** MOTOR CONFIGURATION ****************************
//* ************************************************************************
// Feed motor parameters
const float feedMotorSpeed = 2000.0;           // Steps per second
const float feedMotorAcceleration = 10000.0;   // Steps per second^2 (increased for smoother acceleration)
const float feedMotorSteps = 1320.0;           // Steps per move

// Feed motor pullback parameters (for RETURNING state)
const float feedMotorPullbackSteps = 20.0;     // Steps to pull back during return

// Cut motor parameters
const float cutMotorSpeed = 5000.0;            // Steps per second (reduced for smoother operation)
const float cutMotorAcceleration = 5000.0;    // Steps per second^2 (increased for smooth acceleration)
const float cutMotorSteps = 3000.0;            // Steps per move

// Cut motor return parameters (for RETURNING state)
const float cutMotorReturnSpeed = 10000.0;      // Steps per second (slower for return movement)
const float cutMotorReturnAcceleration = 10000.0; // Steps per second^2 (gentler acceleration for return)

//* ************************************************************************
//* *********************** BUTTON CONFIGURATION ***************************
//* ************************************************************************
// Button debounce time in milliseconds
const unsigned long buttonDebounceTime = 20; 