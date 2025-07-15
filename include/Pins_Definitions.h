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

// Wood detection sensor pin (active LOW - LOW when wood detected)
extern const int IS_WOOD_PIN;

// Pneumatic clamp relay pin (LOW = extended, HIGH = retracted)
extern const int CLAMP_RELAY_PIN;

#endif 