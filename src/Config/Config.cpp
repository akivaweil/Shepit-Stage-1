#include <Arduino.h>
#include "Config.h"

//* ************************************************************************
//* *********************** MOTOR CONFIGURATION ****************************
//* ************************************************************************
// Feed motor parameters
const float feedMotorSpeed = 10000.0;           // Steps per second
const float feedMotorAcceleration = 20000.0;   // Steps per second^2 (increased for smoother acceleration)
const float feedMotorSteps = 0.0;              // Steps per move (set to 0 to disable feeding sequence)

// Feed motor pullback parameters
const float FM_preCutPullback = 0.0;          // Steps to pull back before cut motor moves
const float FM_returnPullback = 0.0;          // Steps to pull back during return (after cutting)

// Reloading sequence timing
const unsigned long woodDetectionToFeedDelay = 10; // Milliseconds to wait after wood detection before feed motor moves

// Wood distance sensor delay
const unsigned long woodDistanceDelay = 10; // Milliseconds to wait after distance sensor triggered before cutting cycle

// Cut motor parameters
const float cutMotorSpeed = 1500.0;            // Steps per second (reduced for smoother operation)
const float cutMotorAcceleration = 10000.0;    // Steps per second^2 (increased for smooth acceleration)
const float cutMotorSteps = 3000.0;            // Steps per move

// Cut motor return parameters (for RETURNING state)
const float cutMotorReturnSpeed = 25000.0;      // Steps per second 
const float cutMotorReturnAcceleration = 50000.0; // Steps per second^2

//* ************************************************************************
//* *********************** BUTTON CONFIGURATION ***************************
//* ************************************************************************
// Button debounce time in milliseconds
const unsigned long buttonDebounceTime = 20; 