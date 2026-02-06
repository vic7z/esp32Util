#ifndef UI_HELPERS_H
#define UI_HELPERS_H

#include "config.h"

// Navigation
void handleListNavigation(ButtonEvent ev, uint8_t& cursor, uint8_t& scroll, uint16_t count, uint8_t visibleRows);

// Formatting
const char* formatMAC(uint8_t* mac);
const char* formatMAC(const char* macStr); // Pass-through for char array MACs
const char* formatElapsed(uint32_t elapsedSec);

// Drawing
void drawRSSIGraph(int8_t* history, uint8_t historyIndex, int8_t minRSSI, int8_t maxRSSI, const char* title, const char* subtitle);
void drawScrollbar(uint8_t scroll, uint16_t count, uint8_t visible);

#endif
