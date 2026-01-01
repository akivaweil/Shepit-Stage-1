#ifndef LOAD_STATE_H
#define LOAD_STATE_H

#include "StateMachine.h"

//* ************************************************************************
//* ************************ LOAD STATE FUNCTIONS ************************
//* ************************************************************************

void enterLoadState();
void updateLoadState();
void exitLoadState();
void emergencyStopFeedOperation();

// Helper functions
void handleWoodSensorDeactivation();
void handleWoodResetWaiting();

#endif // LOAD_STATE_H

