#ifndef CUTTING_STATE_H
#define CUTTING_STATE_H

#include "StateMachine.h"

//* ************************************************************************
//* ************************ CUTTING STATE FUNCTIONS ************************
//* ************************************************************************

void enterCuttingState();
void updateCuttingState();
void exitCuttingState();

// Cutting step functions
void updateCutWoodStep();
void updateReturnCutMotorStep();
void updateCheckConditionsStep();

#endif // CUTTING_STATE_H

