#include <Arduino.h>
#include "Pins_Definitions.h"

//* ************************************************************************
//* *********************** PIN DEFINITIONS *******************************
//* ************************************************************************

// Feed motor pins (original motor)
const int FEED_MOTOR_STEP_PIN = 4;
const int FEED_MOTOR_DIR_PIN = 5;
const int FEED_MOTOR_ENABLE_PIN = 6;

// Cut motor pins (new motor)
const int CUT_MOTOR_STEP_PIN = 15;
const int CUT_MOTOR_DIR_PIN = 16;
const int CUT_MOTOR_ENABLE_PIN = 17;

// Button input pin (active high)
const int BUTTON_PIN = 9;

// Run cycle switch pin (active high)
const int RUN_CYCLE_SWITCH_PIN = 10;

// Reload switch pin (active high)
const int RELOAD_SWITCH_PIN = 41;

// Red button pin (active high)
const int RED_BUTTON_PIN = 39;

// Wood present sensor pin (active LOW - LOW when wood detected)
const int WOOD_PRESENT_SENSOR_PIN = 3;

// Wood distance sensor pin (active HIGH - HIGH when wood detected)
const int WOOD_DISTANCE_SENSOR_PIN = 12;

// Pneumatic clamp relay pin (LOW = extended, HIGH = retracted)
const int CLAMP_RELAY_PIN = 37; 