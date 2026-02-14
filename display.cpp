#include "display.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "menu.h"
#include "utils.h"
#include "security.h"
#include "power.h"

void setRGB(uint32_t color) {
  if (!RGB_ENABLED) return;

  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;

  r = (r * settings.rgbBrightness) / 100;
  g = (g * settings.rgbBrightness) / 100;
  b = (b * settings.rgbBrightness) / 100;

  rgb.setPixelColor(0, rgb.Color(r, g, b));
  rgb.show();
}

void updateRGBStatus() {
  if (!RGB_ENABLED) return;
  if (screenSleeping) return;

  if (attackActive) {
    setRGB(RGB_RED);
  } else if (signalAlert) {
    setRGB(RGB_CYAN);
  } else if (currentScreen == SCREEN_MONITOR) {
    uint8_t load = liveLoad();
    if (load > 70) setRGB(RGB_ORANGE);
    else if (load > 40) setRGB(RGB_YELLOW);
    else setRGB(RGB_GREEN);
  } else if (currentScreen == SCREEN_ANALYZER) {
    setRGB(RGB_BLUE);
  } else if (currentScreen == SCREEN_HIDDEN_SSID) {
    setRGB(RGB_PURPLE);
  } else if (currentScreen == SCREEN_BLE_SCAN) {
    setRGB(RGB_CYAN);
  } else {
    setRGB(RGB_WHITE);
  }
}

void drawGrid(uint8_t x0, uint8_t y0, uint8_t width, uint8_t height) {
  for (int i = 1; i < 4; i++) {
    int y = y0 + (height * i) / 4;
    for (int x = x0; x < x0 + width; x += 4) {
      oled.drawPixel(x, y);
    }
  }
  for (int x = x0 + 32; x < x0 + width; x += 32) {
    for (int y = y0; y < y0 + height; y += 4) {
      oled.drawPixel(x, y);
    }
  }
}

// ── Modern UI helpers ──────────────────────────────────────────────

void drawHeader(const char* title) {
  oled.drawRBox(0, 0, 128, 12, 2);
  oled.setFont(u8g2_font_5x7_tf);
  uint8_t w = oled.getStrWidth(title);
  uint8_t x = (128 - w) / 2;
  oled.setDrawColor(0);
  oled.drawStr(x, 9, title);
  oled.setDrawColor(1);
  // Shadow line for depth
  for (uint8_t sx = 0; sx < 128; sx += 2) {
    oled.drawPixel(sx, 13);
  }
}

void drawFooterPill(uint8_t x, const char* label, bool filled) {
  oled.setFont(u8g2_font_4x6_tf);
  uint8_t w = oled.getStrWidth(label);
  if (filled) {
    oled.drawRBox(x, 55, w + 6, 9, 2);
    oled.setDrawColor(0);
    oled.drawStr(x + 3, 62, label);
    oled.setDrawColor(1);
  } else {
    oled.drawRFrame(x, 55, w + 6, 9, 2);
    oled.drawStr(x + 3, 62, label);
  }
}

void drawFooter(const char* left, const char* right) {
  oled.drawLine(0, 53, 127, 53);
  if (left && strlen(left) > 0) {
    drawFooterPill(1, left, false);
  }
  if (right && strlen(right) > 0) {
    oled.setFont(u8g2_font_4x6_tf);
    uint8_t w = oled.getStrWidth(right);
    drawFooterPill(127 - w - 7, right, true);
  }
}

void drawScrollDots(uint8_t scroll, uint16_t count, uint8_t visible) {
  if (count <= visible) return;
  // Draw scrollbar track
  const uint8_t trackX = 126, trackY = 15, trackH = 37;
  for (uint8_t y = trackY; y < trackY + trackH; y += 2) {
    oled.drawPixel(trackX, y);
  }
  // Draw scrollbar thumb
  uint8_t thumbH = max((uint8_t)4, (uint8_t)(trackH * visible / count));
  uint8_t thumbY = trackY + (uint16_t)scroll * (trackH - thumbH) / (count - visible);
  oled.drawBox(trackX - 1, thumbY, 3, thumbH);
}

// ── Pattern bars ──────────────────────────────────────────────────

void drawPatternBar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t load) {
  if (height == 0) return;
  oled.drawRFrame(x, y, width, height, 1);

  if (load < 20) {
    for (int py = y + 1; py < y + height - 1; py++) {
      for (int px = x + 1; px < x + width - 1; px++) {
        if ((px + py) % 4 == 0) oled.drawPixel(px, py);
      }
    }
  } else if (load < 40) {
    for (int i = 0; i < height + width; i += 3) {
      for (int j = 0; j < height; j++) {
        int px = x + i - j;
        int py = y + j;
        if (px >= x + 1 && px < x + width - 1 && py >= y + 1 && py < y + height - 1) {
          oled.drawPixel(px, py);
        }
      }
    }
  } else if (load < 70) {
    for (int py = y + 1; py < y + height - 1; py++) {
      for (int px = x + 1; px < x + width - 1; px++) {
        if ((px + py) % 2 == 0) oled.drawPixel(px, py);
      }
    }
  } else {
    oled.drawBox(x + 1, y + 1, width - 2, height - 2);
  }
}
