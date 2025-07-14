#include <Arduino.h>
#include "Config.h"

//* ************************************************************************
//* *********************** MOTOR CONFIGURATION ****************************
//* ************************************************************************
// Feed motor parameters
const float feedMotorSpeed = 500.0;           // Steps per second
const float feedMotorAcceleration = 2000.0;   // Steps per second^2
const float feedMotorSteps = 200.0;           // Steps per move

// Cut motor parameters
const float cutMotorSpeed = 100.0;            // Steps per second
const float cutMotorAcceleration = 100.0;    // Steps per second^2
const float cutMotorSteps = 100.0;            // Steps per move

//* ************************************************************************
//* *********************** BUTTON CONFIGURATION ***************************
//* ************************************************************************
// Button debounce time in milliseconds
const unsigned long buttonDebounceTime = 50; 