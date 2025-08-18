#ifndef CONFIG_H
#define CONFIG_H

//* ************************************************************************
//* *********************** MOTOR CONFIGURATION ****************************
//* ************************************************************************
// Feed motor parameters
extern const float feedMotorSpeed;
extern const float feedMotorAcceleration;
extern const float feedMotorSteps;

// Feed motor pullback parameters
extern const float FM_preCutPullback;
extern const float FM_returnPullback;

// Reloading sequence timing
extern const unsigned long woodDetectionToFeedDelay;

// Wood distance sensor delay
extern const unsigned long woodDistanceDelay;

// Cut motor parameters
extern const float cutMotorSpeed;
extern const float cutMotorAcceleration;
extern const float cutMotorSteps;

// Cut motor return parameters (for cutting cycle return movement)
extern const float cutMotorReturnSpeed;
extern const float cutMotorReturnAcceleration;

//* ************************************************************************
//* *********************** BUTTON CONFIGURATION ***************************
//* ************************************************************************
extern const unsigned long buttonDebounceTime;

#endif 