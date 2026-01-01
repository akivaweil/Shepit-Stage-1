#ifndef IDLE_STATE_H
#define IDLE_STATE_H

#include "StateMachine.h"

//* ************************************************************************
//* ************************ IDLE STATE FUNCTIONS ************************
//* ************************************************************************

void enterIdleState();
void updateIdleState();
void exitIdleState();
void resetIdleFeedMotorControl(bool stopMotor);

#endif // IDLE_STATE_H

