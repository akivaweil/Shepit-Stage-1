#include <Arduino.h>
#include "Config.h"

//* ************************************************************************
//* *********************** MOTOR CONFIGURATION ****************************
//* ************************************************************************

// Feed motor parameters
const float feedMotorSpeed = 3000.0;            // Steps per second
const float feedMotorAcceleration = 25000.0;   // Steps per second^2
const float feedMotorSteps = 1000.0;           // Steps per move (for manual commands)

// Wood distance sensor delay
const unsigned long woodDistanceDelay = 10;     // Milliseconds to wait after distance sensor triggered before cutting

// Feed motor timeout
const unsigned long feedMotorTimeout = 7000;    // 7-second safety limit for feed motor operation

// Motor sleep timeout
const unsigned long motorTimeoutMs = 2000;      // 2 seconds for sleep mode

// Motor enable delay
const unsigned long motorEnableDelayMs = 250;   // 250ms motor enable delay

// Cut motor parameters
const float cutMotorSpeed = 750.0;            // Steps per second
const float cutMotorAcceleration = 10000.0;    // Steps per second^2
const float cutMotorSteps = 3500.0;            // Steps per move

// Cut motor return parameters
const float cutMotorReturnSpeed = 30000.0;      // Steps per second 
const float cutMotorReturnAcceleration = 30000.0; // Steps per second^2

//* ************************************************************************
//* *********************** BUTTON CONFIGURATION ***************************
//* ************************************************************************

// Button debounce time in milliseconds
const unsigned long buttonDebounceTime = 20;

//* ************************************************************************
//* *********************** SENSOR DEBOUNCE CONFIGURATION ******************
//* ************************************************************************

// Sensor debounce intervals in milliseconds
const unsigned long sensorDebounceTime = 20;           // Standard sensor debounce (wood, switches, buttons)
const unsigned long distanceSensorDebounceTime = 20;   // Distance sensor specific debounce 