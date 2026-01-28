#ifndef POWER_H
#define POWER_H

#include "config.h"

/* ========== Power Management ========== */

// Update screen timeout and sleep state
void updateScreenTimeout(bool buttonPressed);

// Check if screen should go to sleep and handle sleep/wake
void handleScreenSleep();

// Enter deep sleep mode - only reset button wakes the device
void enterDeepSleep();

/* ========== External Variables ========== */
extern uint32_t lastActivity;
extern bool screenSleeping;

#endif // POWER_H
