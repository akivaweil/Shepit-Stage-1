#include <Arduino.h>

//* ************************************************************************
//* *********************** PIN DEFINITIONS *******************************
//* ************************************************************************

// Feed motor pins (original motor)
#define FEED_MOTOR_STEP_PIN 4
#define FEED_MOTOR_DIR_PIN 5
#define FEED_MOTOR_ENABLE_PIN 6

// Cut motor pins (new motor)
#define CUT_MOTOR_STEP_PIN 15
#define CUT_MOTOR_DIR_PIN 16
#define CUT_MOTOR_ENABLE_PIN 17

// Button input pin (active high)
#define BUTTON_PIN 9

// Pneumatic clamp relay pin (LOW = extended, HIGH = retracted)
#define CLAMP_RELAY_PIN 37 