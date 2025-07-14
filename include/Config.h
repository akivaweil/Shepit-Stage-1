#ifndef CONFIG_H
#define CONFIG_H

//* ************************************************************************
//* *********************** MOTOR CONFIGURATION ****************************
//* ************************************************************************
// Feed motor parameters
extern const float feedMotorSpeed;
extern const float feedMotorAcceleration;
extern const float feedMotorSteps;

// Feed motor return parameters  
extern const unsigned long feedReturnDelayMs;
extern const float feedMotorReturnSteps;

// Cut motor parameters
extern const float cutMotorSpeed;
extern const float cutMotorAcceleration;
extern const float cutMotorSteps;

// Cut motor return parameters (for RETURNING state)
extern const float cutMotorReturnSpeed;
extern const float cutMotorReturnAcceleration;

//* ************************************************************************
//* *********************** BUTTON CONFIGURATION ***************************
//* ************************************************************************
extern const unsigned long buttonDebounceTime;

#endif 