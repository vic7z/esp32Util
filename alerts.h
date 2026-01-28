#ifndef ALERTS_H
#define ALERTS_H

#include "config.h"

void logEvent(uint8_t type, const char* msg);
void updateAlertLED();

extern uint8_t alertLevel;
extern uint32_t lastAlertBlink;
extern bool alertBlinkState;
extern bool signalAlert;
extern uint8_t alertSettingIndex;

#endif // ALERTS_H
