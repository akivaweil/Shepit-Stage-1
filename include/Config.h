#ifndef CONFIG_H
#define CONFIG_H

//* ************************************************************************
//* *********************** MOTOR CONFIGURATION ****************************
//* ************************************************************************

// Feed motor parameters
extern const float feedMotorSpeed;
extern const float feedMotorAcceleration;
extern const float feedMotorSteps;

// Wood distance sensor delay
extern const unsigned long woodDistanceDelay;

// Feed motor timeout
extern const unsigned long feedMotorTimeout;

// Motor sleep timeout
extern const unsigned long motorTimeoutMs;

// Motor enable delay
extern const unsigned long motorEnableDelayMs;

// Cut motor parameters
extern const float cutMotorSpeed;
extern const float cutMotorAcceleration;
extern const float cutMotorSteps;

// Cut motor return parameters
extern const float cutMotorReturnSpeed;
extern const float cutMotorReturnAcceleration;

//* ************************************************************************
//* *********************** BUTTON CONFIGURATION ***************************
//* ************************************************************************

extern const unsigned long buttonDebounceTime;

//* ************************************************************************
//* *********************** SENSOR DEBOUNCE CONFIGURATION ******************
//* ************************************************************************

// Sensor debounce intervals in milliseconds
extern const unsigned long sensorDebounceTime;        // Standard sensor debounce (wood, switches, buttons)
extern const unsigned long distanceSensorDebounceTime; // Distance sensor specific debounce

#endif 