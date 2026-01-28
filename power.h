#ifndef POWER_H
#define POWER_H

#include "config.h"

void updateScreenTimeout(bool buttonPressed);
void handleScreenSleep();
void enterDeepSleep();

extern uint32_t lastActivity;
extern bool screenSleeping;

#endif // POWER_H
