#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"
#include "screens.h"

void setRGB(uint32_t color);
void updateRGBStatus();

void drawGrid(uint8_t x0, uint8_t y0, uint8_t width, uint8_t height);
void drawPatternBar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t load);

void drawHeader(const char* title);
void drawFooter(const char* left, const char* right = nullptr);
void drawFooterPill(uint8_t x, const char* label);
void drawScrollDots(uint8_t scroll, uint16_t count, uint8_t visible);

#endif // DISPLAY_H
