#ifndef SETTINGS_H
#define SETTINGS_H

#include "config.h"

void loadSettings();
void saveSettings();

extern uint8_t settingsCursor;
extern const uint8_t SETTINGS_COUNT;

#endif // SETTINGS_H
