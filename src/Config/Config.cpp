#include <Arduino.h>
#include "Config.h"

//* ************************************************************************
//* *********************** MOTOR CONFIGURATION ****************************
//* ************************************************************************
// Feed motor parameters
const float feedMotorSpeed = 100.0;           // Steps per second
const float feedMotorAcceleration = 400.0;   // Steps per second^2 (increased for smoother acceleration)
const float feedMotorSteps = 200.0;           // Steps per move

// Cut motor parameters
const float cutMotorSpeed = 200.0;            // Steps per second (reduced for smoother operation)
const float cutMotorAcceleration = 100.0;    // Steps per second^2 (increased for smooth acceleration)
const float cutMotorSteps = 200.0;            // Steps per move

//* ************************************************************************
//* *********************** BUTTON CONFIGURATION ***************************
//* ************************************************************************
// Button debounce time in milliseconds
const unsigned long buttonDebounceTime = 50; 