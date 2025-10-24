#ifndef PINS_DEFINITIONS_H
#define PINS_DEFINITIONS_H

//* ************************************************************************
//* *********************** PIN DEFINITIONS *******************************
//* ************************************************************************

// Feed motor pins (original motor)
extern const int FEED_MOTOR_STEP_PIN;
extern const int FEED_MOTOR_DIR_PIN;
extern const int FEED_MOTOR_ENABLE_PIN;

// Cut motor pins (new motor)
extern const int CUT_MOTOR_STEP_PIN;
extern const int CUT_MOTOR_DIR_PIN;
extern const int CUT_MOTOR_ENABLE_PIN;

// Button input pin (active high)
extern const int BUTTON_PIN;

// Run cycle switch pin (active high)
extern const int RUN_CYCLE_SWITCH_PIN;

// Reload switch pin (active high)
extern const int RELOAD_SWITCH_PIN;

// Red button pin (active high)
extern const int RED_BUTTON_PIN;

// Wood present sensor pin (active LOW - LOW when wood detected)
extern const int WOOD_PRESENT_SENSOR_PIN;

// Wood distance sensor pin (active LOW - LOW when wood detected)
extern const int WOOD_DISTANCE_SENSOR_PIN;

// Home switch pin (active LOW - LOW when at home position)
extern const int HOME_SWITCH_PIN;

// Pneumatic clamp relay pin (LOW = extended, HIGH = retracted)
extern const int CLAMP_RELAY_PIN;

#endif 